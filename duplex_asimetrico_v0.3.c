/*
 * Aplicación de proba "full-duplex"
 *
 * Envía datos aleatorios ao extremo remoto, que actúa como "espello", 
 * e comproba que os datos recibidos se corresponden cos enviados.
 *
 * Marca os erros con un "X", e os datos recibidos tal como se enviaron 
 * con un "·"
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "interfaz.h"
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#define MILLION_F ((float)(1000000))

/*
   Crea un novo "struct timeval" co resultado de X - Y 
   Devolve 1 se a diferencia é negativa (é dicir, Y > X), 
   noutro caso devolve 0.

http://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html
*/
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
{
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }

  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  return x->tv_sec < y->tv_sec;
}

static const char *str_errors[] = { 
  "EXOK", "EXBADTID", "EXCDUP", "EXNET", "EXKERNEL", "EXMAXDATA", "EXNOTSAP", "EXNODATA",
  "EXCLOSE", "EXINVA",  "EXMAXC", "EXUNDEF", "EXDISC"
};

static const char *erro_a_cadea(int erro) {
  int index = -erro;

  if (index >= 0 && index < sizeof(str_errors))
    return str_errors[index];
  else
    return "Erro descoñecido";
}

static void uso(const char *progname) {
  fprintf(stderr, "Uso: %s [-s] [-c ip_dst] [-l porto local] [-r porto remoto] \n", progname);
  fprintf(stderr, "\n Exemplo: \n");
  fprintf(stderr, "	Cliente: 	aplic -c 172.19.45.17 -r 3333\n");
  fprintf(stderr, "	Servidor: 	aplic -s -l 3333 \n");
  exit(-1);
}

static void mostra_tsaps(const t_direccion *orixe, const t_direccion *destino) {
  fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  fprintf(stderr, "IP Orixe: %s\tPorto Orixe: %d\n", inet_ntoa(orixe->ip), orixe->puerto);
  fprintf(stderr, "IP Destino: %s\tPorto Destino: %d\n", inet_ntoa(destino->ip), destino->puerto);
  fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}

static char *init_noise(char *buf, size_t len) {
  int32_t *buf32 = (int32_t *)buf;
  int i;

  for (i = 0; i < len/4; i++) {
    buf32[i] = random();
  }

  return buf;
}

static char *gen_noise(char *buf, size_t len) {
  int32_t noise = random();
  int i;
  int32_t *buf32 = (int32_t *)buf;

  for (i = 0; i < len/4; i++) {
    buf32[i] ^= noise;
  }

  return buf;
}


/*
 * do_receiver_part: Implementa a recepción e reenvío dos datos recibidos desde 
 *                   un cliente remoto.
 *      - dir_local:    TSAP local
 *      - dir_remota:   TSAP remota
 */
static int do_receiver_part(t_direccion *dir_local, t_direccion *dir_remota) {
  int cx1;
  int8_t flagsi, flagso;
  int i = 0;
  int len = 0;
  int bytesrecibidos = 0;
  int bs = 200;
  char buf[8192];

  cx1 = t_listen(dir_local, dir_remota);
  if (cx1 < 0) {
    fprintf(stderr, "Erro en t_listen: %s\n", erro_a_cadea(cx1));
    return cx1;
  }

  mostra_tsaps(dir_local, dir_remota);

  fprintf(stderr, "[Realizando reenvío de paquetes recibidos (\"loopback\")] -> Procesados ");

  do {
    flagsi = 0;

    /* Agardamos a ter "bs" datos */
    len = t_receive(cx1, buf, bs, &flagsi);
    if (len < 0) {
      fprintf(stderr, "\nErro recibindo: %s", erro_a_cadea(len));
      return len;
    }
    if (flagsi & CLOSE) {
      fprintf(stderr, "\nConexión entrante pechada\n");
    }

    bytesrecibidos += len; 
    fprintf(stderr, "%10d bytes (total: %10d)", len, bytesrecibidos);

    /* 
     * Transmitimos todos os datos recibidos. Evitamos pechar este sentido da conexión. 
     */
    flagso = 0;
    len = t_send(cx1, buf, len, &flagso);
    if (len < 0 ) {
      fprintf(stderr, "\nErro en envío: %s ", erro_a_cadea(len));
      return len;
    } 

    fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");

    bs = (bs + 256)%sizeof(buf);

  } while( !(flagsi & CLOSE) );

  fprintf(stderr, "\nFinalizado receptor\n");
  fprintf(stderr, "\nEnviando datos extra...\n");
  for (i = 0; i<8192; i++)
    buf[i] = i%256; 
  flagso |= CLOSE;
  len = t_send(cx1, buf, 8192, &flagso);
  if (len < 0) {
    fprintf(stderr, "Erro: %s\n", erro_a_cadea(len));
    return len; 
  }

  t_disconnect(cx1);
  if (len < 0 )
    return len;
  else 
    return EXOK;
}

/*
 * do_sender_part: Implementa o envío e recepción de datos aleatorios a un servidor que realiza
 *                 eco. Comproba que os datos recibidos son idénticos aos enviados.
 *      - dir_local:    TSAP local
 *      - dir_remota:   TSAP remota
 *      - len:          Cantidade de datos
 */
static int do_sender_part(t_direccion *dir_local, const t_direccion *dir_remota, size_t len) {
  int cx1;
  int8_t flags = 0;
  int i = 0;
  int bs = 200;
  int len1 = 0, erros = 0, bytesenviados = 0;
  double bytessec = 0;
  char data[8192];
  char buf[sizeof(data)];
  struct timeval start, end, elapsed;

  cx1 = t_connect(dir_remota, dir_local);
  if (cx1 < 0) {
    fprintf(stderr, "Erro en t_connect: %s\n", erro_a_cadea(cx1));
    return cx1;
  }

  mostra_tsaps(dir_local, dir_remota);

  init_noise(data, sizeof(data));

  gettimeofday(&start, NULL);

  do {
    gen_noise(data, sizeof(data));

    /* Ultimo bloque */
    if (bs >= len) {
      fprintf(stderr, "\n[Faltan %d bytes por transmitir][Enviando CLOSE]\n", len);
      flags |= CLOSE;
      bs = len;
    }

    len1 = t_send(cx1, data, bs, &flags);
    if (len1 < 0) {
      fprintf(stderr, "\nErro en envío: %s\n", erro_a_cadea(len1));
      return len1;
    } 

    len -= bs;
    bytesenviados += len1;

    /* Esperamos ata recibir os mesmos datos enviados */
    len1 = t_receive(cx1, buf, bs, &flags);
    if (len1 < 0) {
      fprintf(stderr, "Erro: %s\n", erro_a_cadea(len1));
      return len1; 
    }

    gettimeofday(&end, NULL);
    timeval_subtract(&elapsed, &end, &start);
    bytessec = bytesenviados  / (elapsed.tv_sec + elapsed.tv_usec/MILLION_F);

    fprintf(stderr, "%10d bytes (%10.0f bytes/s) ", bytesenviados, bytessec );
    fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");

    /* Comprobamos se os datos son correctos */
    if (memcmp(data, buf, bs)) {
      fprintf(stderr, "X"); // Erro
      erros++;
    } else
      fprintf(stderr, "·"); // Ok

    bs = (bs + 256)%sizeof(buf);

  } while (len > 0);

  fprintf(stderr, "\nFinalizando transmisor\n");
  fprintf(stderr, "\nErros: %d\n", erros);

  fprintf(stderr, "\nRecibindo datos extra...\n");
  len1 = t_receive(cx1, buf, 8192, &flags);
  if (len1 < 0) {
    fprintf(stderr, "Erro: %s\n", erro_a_cadea(len1));
    return len; 
  }
  for (i = 0; i<8192; i++) 
    if (buf[i] != (char) i%256) 
      fprintf(stderr, "X");
    else 
      fprintf(stderr, "·");
  fprintf(stderr, "\n\n");

  t_disconnect(cx1);
  if (len1 < 0 )
    return len;
  else 
    return EXOK;
}

int main(int argc, char *argv[]) {
  bool server = false, client = false;
  t_direccion dir_orixe = {{INADDR_ANY}, 0}, dir_destino = {{INADDR_ANY}, 0};
  int erro = 0, opt;

  fprintf(stderr, "\n¡ATENCIÓN! Esta aplicación proporciónase como unha axuda\n");
  fprintf(stderr, "           para a depuración. O seu uso non implica\n");
  fprintf(stderr, "           ningunha garantía de cara á avaliación en LTM\n\n");

  while ((opt = getopt(argc, argv, "sc:l:r:")) != -1) {
    switch (opt) {
      case 's': //servidor
        server = true;
        break;
      case 'c': //cliente
        client = true;
        inet_aton(optarg, &dir_destino.ip);
        break;
      case 'l': //porto local
        dir_orixe.puerto = atoi(optarg);
        break;
      case 'r': //porto remoto
        dir_destino.puerto = atoi(optarg);
        break;
      default:
        uso(basename(argv[0]));
        return -1;
    }
  }

  if (server)
    erro = do_receiver_part(&dir_orixe, &dir_destino);
  else if (client)
    erro = do_sender_part(&dir_orixe, &dir_destino, 100000000UL);
  else 
    uso(basename(argv[0]));

  if (erro < 0)
    fprintf(stderr, "Acabamos co erro: %s\n", erro_a_cadea(erro));
  return 0;
}
