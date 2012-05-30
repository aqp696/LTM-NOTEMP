/* 
 * Aplicación de proba para condicións de erro
 *
 * ¡ATENCIÓN! Esta aplicación proporciónase como unha axuda para a depuración. O seu uso non implica
 * ningunha garantía de cara á avaliación en LTM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <getopt.h>
#include "interfaz.h"

static const char *str_errors[] = { 
  "EXOK", "EXBADTID", "EXCDUP", "EXNET", "EXKERNEL", "EXMAXDATA", "EXNOTSAP", "EXNODATA",
  "EXCLOSE", "EXINVA",  "EXMAXC", "EXUNDEF", "EXDISC"
};

static const char *erro_a_cadea(int error) {
  int index = -error;

  if (index >= 0 && index < sizeof(str_errors))
    return str_errors[index];
  else
    return "Erro descoñecido";
}

static void uso(const char *progname) {
  fprintf(stderr, "Uso:\n" );
  fprintf(stderr, "	Cliente:    %s --cliente  --ip-local [IP] --ip-remota [IP] --porto-local [Porto] --porto-remoto [Porto]\n", progname);
  fprintf(stderr, "	Servidor:   %s --servidor --ip-local [IP] --ip-remota [IP] --porto-local [Porto] --porto-remoto [Porto]\n", progname);
  exit(-1);
}

static void mostra_tsaps(const t_direccion *local, const t_direccion *remoto) {
  fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  fprintf(stderr, "IP local: %s\tPorto local: %d\n", inet_ntoa(local->ip), local->puerto);
  fprintf(stderr, "IP remota: %s\tPorto remoto: %d\n", inet_ntoa(remoto->ip), remoto->puerto);
  fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}


static int do_receiver_part(t_direccion *dir_local, t_direccion *dir_remota) {
  int8_t flags = 0;
  char buf[2000];
  int cx[NUM_MAX_CXs + 2];
  int i = 0, len = 0; 

  fprintf(stderr, "Agardando unha conexión correcta...\n");
  /* Conexión correcta */
  cx[i] = t_listen(dir_local, dir_remota);
  if (cx[i] < 0) {
    fprintf(stderr, "Erro en t_listen: %s\n", erro_a_cadea(cx[i]));
    return cx[i];
  }
  fprintf(stderr, "Conexión realizada\n");

  mostra_tsaps(dir_local, dir_remota);

  i++;

  fprintf(stderr, "Comprobando colisión con unha conexión existente...\n");
  /* Colisión con conexión existente */
  cx[i] = t_listen(dir_local, dir_remota);
  if ( cx[i] != EXCDUP ) {
    fprintf(stderr, "t_listen non devolveu un EXCDUP para unha conexión duplicada\n");
    return cx[i];
  } else {
    /* OK */
    fprintf(stderr, "Colisión detectada correctamente\n");
  }

  fprintf(stderr, "Agardando %d conexións correctas...\n", NUM_MAX_CXs-1);
  /* Conexións correctas */
  for (; i<NUM_MAX_CXs; i++) {
    dir_local->puerto++;
    dir_remota->puerto = 0;
    cx[i] = t_listen(dir_local, dir_remota);
    if (cx[i] < 0) {
      fprintf(stderr, "Erro en t_listen inesperado: %s\n", erro_a_cadea(cx[i]));
      return cx[i];
    }
  }
  fprintf(stderr, "Conexións completadas satisfactoriamente\n");

  fprintf(stderr, "Comprobando a detección do erro EXMAXC...\n");
  dir_local->puerto++;
  dir_remota->puerto = 0;
  cx[i] = t_listen(dir_local, dir_remota);
  if ( cx[i] != EXMAXC ) {
    fprintf(stderr, "t_listen non devolveu un EXMAXC ao alcanzar o máximo de conexións\n");
    return cx[i];
  } else {
    /* OK */
    fprintf(stderr, "Erro EXMAXC comprobado correctamente\n");
  }


  /* Recepción de datos en conexión errónea */
  fprintf(stderr,"Comprobando t_receive sobre conexión inexistente...\n");
  len = t_receive(cx[i], buf, 200, &flags);
  if ( len != EXBADTID ) {
    fprintf(stderr, "t_receive non devolveu un EXBADTID ao utilizar unha conexión inexistente\n");
    return len;
  } else {
    /* OK */
    fprintf(stderr, "Erro EXBADTID comprobado correctamente\n");      
  }

  /* Parámetros incorrectos */
  fprintf(stderr, "Comprobando a detección de erros nos parámetros de t_receive...\n");
  len = t_receive(cx[--i], buf, -50, &flags);
  if ( len != EXINVA ) {
    fprintf(stderr, "t_receive non devolveu un EXINVA ao utilizar un tamaño negativo\n");
    return len;
  } else {
    /* OK */
    fprintf(stderr, "t_receive detectou correctamente un tamaño negativo\n");            
  }

  /* Parámetros incorrectos */
  fprintf(stderr, "Comprobando a detección de erros nos parámetros de t_receive...\n");        
  len = t_receive(cx[i], buf, 50, NULL);
  if ( len != EXINVA ) {
    fprintf(stderr, "t_receive non devolveu un EXINVA ao utilizar uns flags sen iniciar\n");
    return len;
  } else {
    /* OK */
    fprintf(stderr, "t_receive detectou correctamente un campo flags apuntanto a NULL\n");                  
  }

  /* Non debería afectar */
  flags = 0b10000000;

  /* Peche de conexión */
  flags |= CLOSE;

  fprintf(stderr, "Comprobando a recepción correcta dunha pequena mensaxe e o peche da conexión...\n");            
  len = t_receive(cx[i], buf, 200, &flags);
  if (len == 200 ) {
    fprintf(stderr, "Recibida mensaxe %s\n", buf); 
  } else {
    fprintf(stderr, "Problema en t_receive\n");
    return len;
  }

  /* flags debería quedar limpo */
  fprintf(stderr, "Comprobando se t_receive deixa o bit CLOSE a 0 despois de ter solicitado un peche de conexión...\n");            
  if ( ( flags & CLOSE ) ) {
    fprintf(stderr, "Erro en t_receive, a variable \"flags\" debería ter reiniciado o bit CLOSE\n");
    flags = 0;
  } else {
    fprintf(stderr, "t_receive reiniciou correctamente a variable \"flags\"\n");
  }

  /* Erro EXCLOSE */
  fprintf(stderr, "Comprobando se t_send devolve un EXCLOSE despois de ter pechada a conexión...\n");            
  len = t_send(cx[i], buf, 200, &flags);
  if ( len != EXCLOSE ) {
    fprintf(stderr, "Erro en t_send, debería ter emitido un EXCLOSE\n");
  } else {
    /* OK */
    fprintf(stderr, "t_send emitiu correctamente o erro EXCLOSE\n");
  }

  /* Desconexión */
  fprintf(stderr, "Comprobando se t_receive avisa sobre o peche abrupto da conexión...\n");            
  len = t_receive(cx[i], buf, 200, &flags);
  if (len != EXDISC ) {
    fprintf(stderr, "Erro en t_receive, debería ter emitido un EXDISC\n");
  } else {
    /* OK */
    fprintf(stderr, "t_send emitiu correctamente o erro EXCLOSE\n");
  }

  /* Desconexión dunha conexión inexistente */
  fprintf(stderr, "Comprobando se t_disconnect indica que se quere pechar unha conexión inexistente...\n");            
  len = t_disconnect(cx[++i]);
  if ( len != EXBADTID ) {
    fprintf(stderr, "t_receive non devolveu un EXBADTID ao utilizar unha conexión inexistente\n");
    return len;
  } else {
    /* OK */
    fprintf(stderr, "t_disconnect emitiu correctamente o erro EXBADTID\n");
  }

  printf("\n\n Finalizada proba no lado de recepción\n");
  return EXOK;
}

static int do_sender_part(t_direccion *dir_local, t_direccion *dir_remota) {
  int8_t flags = 0;
  char buf[2000];
  int cx[NUM_MAX_CXs + 2];
  int i = 0, len = 0; 
  t_direccion dir_tmp;


  /* Conexión correcta */
  fprintf(stderr, "Creando unha conexión...\n");    
  cx[i] = t_connect(dir_remota, dir_local);
  if (cx[i] < 0) {
    fprintf(stderr, "Erro en t_connect inesperado: %s\n", erro_a_cadea(cx[i]));
    return cx[i];
  } 
  fprintf(stderr, "Conexión efectuada correctamente\n");

  mostra_tsaps(dir_local, dir_remota);

  i++;

  /* Colisión con conexión existente */
  fprintf(stderr, "Comprobando colisión con unha conexión existente...\n");    
  cx[i] = t_connect(dir_remota, dir_local);
  if ( cx[i] != EXCDUP ) {
    fprintf(stderr, "t_connect non devolveu un EXCDUP para unha conexión duplicada\n");
    return cx[i];
  } else {
    /* OK */
    fprintf(stderr, "Colisión detectada correctamente\n");
  }

  /* Conexión con porto erróneo */
  fprintf(stderr, "Realizando conexión con un porto erróneo...\n");
  memcpy(&dir_tmp, dir_remota, sizeof(t_direccion));
  dir_tmp.puerto--;
  dir_local->puerto = 0;
  cx[i] = t_connect(&dir_tmp, dir_local);
  if ( cx[i] != EXNOTSAP ) {
    fprintf(stderr, "t_connect non devolveu un EXNOTSAP para unha conexión con un porto erróneo\n");
    return cx[i];
  } else {
    /* OK */
    fprintf(stderr, "Erro EXNOTSAP comprobado correctamente\n");
  }

  /* Conexión con IP errónea */
  fprintf(stderr, "Realizando conexión con unha IP errónea...\n");
  dir_tmp.ip.s_addr++;
  dir_local->puerto = 0;
  cx[i] = t_connect(&dir_tmp, dir_local);
  if ( cx[i] != EXNET ) {
    fprintf(stderr, "t_connect non devolveu un EXNET para unha conexión con unha IP errónea\n");
    return cx[i];
  } else {
    /* OK */
    fprintf(stderr, "Erro EXNET comprobado correctamente\n");      
  }

  /* Conexións correctas */
  fprintf(stderr, "Realizando %d conexións correctas...\n", NUM_MAX_CXs-1);
  for (; i<NUM_MAX_CXs; i++) {
    dir_remota->puerto++;
    dir_local->puerto = 0;
    usleep(200000);
    cx[i] = t_connect(dir_remota, dir_local);
    if (cx[i] < 0) {
      fprintf(stderr, "Erro en t_connect inesperado: %s\n", erro_a_cadea(cx[i]));
      return cx[i];
    }
  }
  fprintf(stderr, "Conexións completadas satisfactoriamente\n");

  fprintf(stderr, "Comprobando a detección do erro EXMAXC...\n");
  dir_remota->puerto++;
  dir_local->puerto = 0;
  cx[i] = t_connect(dir_remota, dir_local);
  if ( cx[i] != EXMAXC ) {
    fprintf(stderr, "t_connect non devolveu un EXMAXC ao alcanzar o máximo de conexións\n");
    return cx[i];
  } else {
    /* OK */
    fprintf(stderr, "Erro EXMAXC comprobado correctamente\n");
  }


  /* Envío de datos en conexión errónea */
  fprintf(stderr,"Comprobando t_send sobre conexión inexistente...\n");
  len = t_send(cx[i], buf, 200, &flags);
  if ( len != EXBADTID ) {
    fprintf(stderr, "t_send non devolveu un EXBADTID ao utilizar unha conexión inexistente\n");
    return cx[i];
  } else {
    /* OK */
    fprintf(stderr, "Erro EXBADTID comprobado correctamente\n");      
  }


  /* Parámetros incorrectos */
  fprintf(stderr, "Comprobando a detección de erros nos parámetros de t_send...\n");
  len = t_send(cx[--i], buf, -50, &flags);
  if ( len != EXINVA ) {
    fprintf(stderr, "t_send non devolveu un EXINVA ao utilizar un tamaño negativo\n");
    return len;
  } else {
    /* OK */
    fprintf(stderr, "t_send detectou correctamente un tamaño negativo\n");            
  }


  /* Parámetros incorrectos */
  fprintf(stderr, "Comprobando a detección de erros nos parámetros de t_send...\n");    
  len = t_send(cx[i], buf, 50, NULL);
  if ( len != EXINVA ) {
    fprintf(stderr, "t_send non devolveu un EXINVA ao utilizar uns flags sen iniciar\n");
    return len;
  } else {
    /* OK */
    fprintf(stderr, "t_send detectou correctamente un campo flags apuntanto a NULL\n");                  
  }

  /* Non debería afectar */
  flags = 0b10000000;

  fprintf(stderr, "Comprobando o envío dunha pequena mensaxe...\n");            
  strcpy(buf, "Mensaxe de proba");

  len = t_send(cx[i], buf, 200, &flags);
  if (len == 200 ) {
    fprintf(stderr, "Enviada a mensaxe %s\n", buf); 
  } else {
    fprintf(stderr, "Fallo inesperado en t_send\n");
    return len;
  }

  usleep(200000);
  fprintf(stderr, "Comprobando a recepción da notificación de CLOSE en t_send...\n");                
  len = t_send(cx[i], buf, 1, &flags);
  if ( !(flags & CLOSE) ) {
    fprintf(stderr, "Erro en t_send, non indicou nos flags que o outro extremo pechou a conexión\n");
  } else {
    fprintf(stderr, "t_send notificou correctamente que o outro extremo pechou a conexión\n");
  }

  fprintf(stderr, "Comprobando se t_disconnect indica que se quere pechar unha conexión inexistente...\n");
  len = t_disconnect(cx[i]);
  if ( len != EXBADTID ) {
    fprintf(stderr, "t_receive non devolveu un EXBADTID ao utilizar unha conexión inexistente\n");
    return len;
  } else {
    /* OK */
    fprintf(stderr, "t_disconnect emitiu correctamente o erro EXBADTID\n");
  }

  fprintf(stderr, "Pechando unha conexión...\n");    
  len = t_disconnect(cx[i]);
  if ( len < 0 ) {
    fprintf(stderr, "Erro no t_disconnect\n");
    return len;
  } else {
    /* OK */
  }

  printf("\n\n Finalizada proba no lado emisor\n");

  return EXOK;
}

int main(int argc, char *argv[]) {
  static int server = 0;
  t_direccion dir_local = {{INADDR_ANY}, 0}, dir_remota = {{INADDR_ANY}, 0};
  int opt;

  fprintf(stderr, "\n¡ATENCIÓN! Esta aplicación proporciónase como unha axuda\n");
  fprintf(stderr, "           para a depuración. O seu uso non implica\n");
  fprintf(stderr, "           ningunha garantía de cara á avaliación en LTM\n\n");



  while (1) { 
    static struct option long_options[] =
    {
      {"cliente",         no_argument,       &server, 1},
      {"servidor",        no_argument,       &server, 2},
      {"ip-local",        required_argument, 0, 'l'},
      {"ip-remota",       required_argument, 0, 'r'},
      {"porto-local",     required_argument, 0, 'p'},
      {"porto-remoto",    required_argument, 0, 'q'},
      {0, 0, 0, 0}
    };
    int option_index = 0;
    opt = getopt_long(argc, argv, "scl:r:p:q:", long_options, &option_index);
    if ( opt == -1 )
      break;
    switch (opt) {
      case 0:
        break;
      case 'l': //ip local
        inet_aton(optarg, &dir_local.ip);
        break;
      case 'r': //ip remota
        inet_aton(optarg, &dir_remota.ip);
        break;
      case 'p': //porto local
        dir_local.puerto = atoi(optarg);
        break;
      case 'q': //porto remoto
        dir_remota.puerto = atoi(optarg);
        break;
      case ':':   // falta argumento
        fprintf(stderr, "%s: A opción `-%c' precisa un argumento\n", argv[0], optopt);
        break;
      case '?':
      default:
        uso(basename(argv[0]));
        return -1;
    }
  }

  if (server == 2) // servidor
    return do_receiver_part(&dir_local, &dir_remota);
  else if (server == 1) // cliente
    return do_sender_part(&dir_local, &dir_remota);
  else {
    uso(basename(argv[0]));
    return 0;
  }
}
