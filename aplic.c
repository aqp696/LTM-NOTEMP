#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "interfaz.h"
#include <sys/time.h>

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

static const char *error_to_str(int error) {
    int index = -error;

    if (index >= 0 && index < sizeof(str_errors))
        return str_errors[index];
    else
        return "Erro descoñecido";
}

static void uso(const char *progname) {
    fprintf(stderr, "Uso: %s [-s] [-c ip_dst] [-l porto_local] [-r porto_remoto] [-f ficheiro]\n", progname);
    fprintf(stderr, "\n Exemplo: \n");
    fprintf(stderr, "	Cliente: 	aplic -c 172.19.45.17 -r 3333 -f test1.bin\n");
    fprintf(stderr, "	Servidor: 	aplic -s -l 3333 -f recibido.bin\n\n");

    exit(-1);
}

static void mostra_tsaps(const t_direccion *orixe, const t_direccion *destino) {
    fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    fprintf(stderr, "IP Orixe: %s\tPorto Orixe: %d\n", inet_ntoa(orixe->ip), orixe->puerto);
    fprintf(stderr, "IP Destino: %s\tPorto Destino: %d\n", inet_ntoa(destino->ip), destino->puerto);
    fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}

/*
 * do_receiver_part: Implementa a recepción dun ficheiro desde un cliente remoto.
 *      - dir_local:    TSAP local
 *      - dir_remota:   TSAP remota
 *      - fich:         Nome do ficheiro de destino
 */
static int do_receiver_part(t_direccion *dir_local, t_direccion *dir_remota, char* fich) {
    int cx1 = 0, len1 = 0, bytesrecibidos = 0;
    int8_t flags = 0; 
    int bs = 200;
    FILE *fwri;
    char buf[8192];

    cx1 = t_listen(dir_local, dir_remota);
    if (cx1 < 0) {
        fprintf(stderr, "Erro en t_listen: %s\n", error_to_str(cx1));
        return cx1;
    }

    mostra_tsaps(dir_local, dir_remota);
    
    fprintf(stderr,"\nAntes de abrir fichero");

    if((fwri = fopen(fich,"w"))<0){
        fprintf(stderr, "Non se puido abrir o ficheiro: %s\n", fich);
        exit(1);
    }
    
    fprintf(stderr,"\nENTRO AKI1!!!");

    do {

        len1 = t_receive(cx1, buf, bs, &flags);
        fprintf(stderr,"\nterminado un receive, len1: %d",len1);

        if (len1 > 0 ) {
            bytesrecibidos += len1;
            fprintf(stderr, "Recibidos %10d bytes", bytesrecibidos); 
            fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
            fwrite(buf, 1, len1, fwri);
        }

        bs = (bs + 256)%sizeof(buf);

    } while(len1 > 0 && !(flags & CLOSE) );

    if (len1 < 0 && len1 != EXCLOSE && len1 != EXDISC) {
          t_disconnect(cx1);
          fprintf(stderr, "Produciuse un erro (%s) durante a recepción do ficheiro", error_to_str(len1));
          return len1;
    }

    printf("\n\n Finalizada recepción\n");
    fclose(fwri);
    //t_disconnect(cx1);
    return EXOK;
}

/*
 * do_sender_part: Implementa o envío dun ficheiro a un servidor remoto.
 *      - dir_local:    TSAP local
 *      - dir_remota:   TSAP remota
 *      - fich:         Nome do ficheiro que se desexa enviar
 */
static int do_sender_part(t_direccion *dir_local, const t_direccion *dir_remota, char* fich) {
    int cx1 = 0, len1 = 0, lenfich = 0, bytesenviados = 0;
    float bytessec = 0;
    int8_t flags = 0;
    char data[8192];
    FILE *fre;
    struct timeval start, end, elapsed;
    int bs = 200;

    cx1 = t_connect(dir_remota, dir_local);
    if (cx1 < 0) {
        fprintf(stderr, "Error en t_connect: %s\n", error_to_str(cx1));
        return cx1;
    }

    mostra_tsaps(dir_local, dir_remota);

    if((fre = fopen(fich,"r"))<0){
        fprintf(stderr, "Non se puido ler o ficheiro: %s\n", fich);
        exit(1);
    }

    gettimeofday(&start, NULL);

    do {
        char buf[sizeof(data)];

        lenfich = fread(data, 1, bs, fre);
        if (feof(fre)) {
            fprintf(stderr, "Rematouse o envío do ficheiro. Pechamos a conexión\n");
            flags |= CLOSE;
        }

        len1 = t_send(cx1, data, lenfich, &flags);

        if (len1 < 0) {
            t_disconnect(cx1);
            fprintf(stderr, "Produciuse un erro durante o envío dos datos: %s\n", error_to_str(len1));
            return len1;
        } else {
            bytesenviados += len1;
            gettimeofday(&end, NULL);
            timeval_subtract(&elapsed, &end, &start);
            //fprintf(stderr, "segundos %d, usec %d\n", elapsed.tv_sec, elapsed.tv_usec);
            bytessec = bytesenviados  / (elapsed.tv_sec + elapsed.tv_usec/MILLION_F);
            fprintf(stderr, "Enviados  %10d bytes (%10.0f B/s)", bytesenviados, bytessec); 
            fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
        }

        bs = (bs + 256)%sizeof(buf);

    } while (!feof(fre));

    fprintf(stderr, "\n\nFinalizouse o envío.\n");
    fclose(fre);
    sleep(5);
    fprintf(stderr, "Elminando a conexión!\n");
    t_disconnect(cx1);
    return EXOK;
}

int main(int argc, char *argv[]) {
    bool server = false, cliente = false;
    t_direccion dir_orixe = {INADDR_ANY, 0}, dir_destino = {INADDR_ANY, 0};
    char nomefich[256];
    int error, opt;

    while ((opt = getopt(argc, argv, "sc:l:r:f:")) != -1) {
        switch (opt) {
            case 's': //servidor
                server = true;
                break;
            case 'c': //cliente
                cliente = true;
                inet_aton(optarg, &dir_destino.ip);
                break;
            case 'l': //porto local
                dir_orixe.puerto = atoi(optarg);
                break;
            case 'r': //porto remoto
                dir_destino.puerto = atoi(optarg);
                break;
            case 'f': //nome de ficheiro
                strcpy(nomefich, optarg);
                break;
            default:
                uso(basename(argv[0]));
                return -1;

        }
    }

    if (server)
        error = do_receiver_part(&dir_orixe, &dir_destino, nomefich);
    else if (cliente)
        error = do_sender_part(&dir_orixe, &dir_destino, nomefich);
    else 
        uso(basename(argv[0]));

    if (error < 0)
        fprintf(stderr, "Acabamos con un erro: %s\n", error_to_str(error));

    return 0;
}
