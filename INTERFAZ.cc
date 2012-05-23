#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "interfaz.h"
#include "CONFP.h"

kernel_shm_t * KERNEL = NULL;
char dir_proto[64] = KERNEL_MEM_ADDR;

int t_connect(const t_direccion *tsap_destino, t_direccion *tsap_origen) {
    int res = EXOK;
    t_direccion tsap_destino_aux;
    list<buf_pkt>::iterator it_buffer;
    list<buf_pkt>::iterator it_tx;
#ifdef DEPURA
    fprintf(stderr,"\nObtenemos el Kernel");
#endif
    //entrar en KERNEL
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
    if (er < 0)
        return EXKERNEL;

    fprintf(stderr,"\nComprobamos parametros");
    // comprobar parametros
    if (comprobar_parametros(tsap_destino,tsap_origen,'c') == -1) {
        fprintf(stderr,"\nTSAPS incorrecto");
        ltm_exit_kernel((void**) &KERNEL);
        return EXINVA;
    }
    
    pid_t pid = gettid();

    bloquear_acceso(&KERNEL->SEMAFORO);

    fprintf(stderr,"\nMiramos si hay espacio para una nueva conexion");
    //miramos si hay espacio para una nueva conexion
    if(KERNEL->num_CXs == NUM_MAX_CXs){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXMAXC;
    }

    fprintf(stderr,"\nMiramos si el connect no esta repetido");
    if(buscar_connect_repetido(tsap_origen,tsap_destino)>-1){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXCDUP;
    }

    fprintf(stderr,"\nBuscamos celda libre");
    //localizar celda libre, elimino la conexion libre y aumento el numero de conexiones.
    int indice_celda = buscar_celda_libre();
    KERNEL->CXs_libres.pop_front();
    KERNEL->num_CXs++;

    
    //int indice_celda = KERNEL->indice_libre;//siempre tendremos el indice libre mas bajo

    fprintf(stderr,"\nRellenamos los datos de la conexion");
    //BLOQUEO
    inicia_barrera(&KERNEL->CXs[indice_celda].barC);

    KERNEL->CXs[indice_celda].ap_pid = pid;
    KERNEL->CXs[indice_celda].estado_cx = CONNECT;
    KERNEL->CXs[indice_celda].celda_ocupada= 1;
    //no estoy seguro de si necesito meter estos datos??
    KERNEL->CXs[indice_celda].ip_destino = tsap_destino->ip;
    KERNEL->CXs[indice_celda].ip_local = tsap_origen->ip;
    KERNEL->CXs[indice_celda].puerto_origen = tsap_origen->puerto;
    KERNEL->CXs[indice_celda].puerto_destino = tsap_destino->puerto;
    desbloquear_acceso(&KERNEL->SEMAFORO);

    fprintf(stderr,"\ncreamos el pakete");
    //creamos el paquete
    //usamos tsap_destino_aux porque este ya no es const y asi crear_pkt ya no es const
    it_buffer = buscar_buffer_libre();
    it_buffer->contador_rtx = NUM_MAX_RTx;
    it_tx = KERNEL->CXs[indice_celda].TX.end();
    KERNEL->CXs[indice_celda].TX.splice(it_tx,KERNEL->buffers_libres,it_buffer);
    fprintf(stderr,"\nya hicimos el splice");
    it_tx = KERNEL->CXs[indice_celda].TX.end();
    memcpy(&tsap_destino_aux,tsap_destino,sizeof(t_direccion));
    fprintf(stderr,"\nvamos a llamar a crear_pkt");
    crear_pkt(*(it_tx->pkt),CR,&tsap_destino_aux,tsap_origen,NULL,0,indice_celda,0);
    fprintf(stderr,"\nEnviamos el pakete y nos bloqueamos");
    //enviamos el paquete y nos bloqueamos
    enviar_tpdu(tsap_destino->ip,it_tx->pkt,sizeof(tpdu));
    fprintf(stderr,"\nla id_local es: %d",indice_celda);
    bloquea_llamada(&KERNEL->CXs[indice_celda].barC);
    
    bloquear_acceso(&KERNEL->SEMAFORO);//bloqueamos para leer de kernel
    res = KERNEL->CXs[indice_celda].resultado_peticion;

    fprintf(stderr,"\nComprobamos si hubo algun error en la conexion");
    //comprobamos si hubo algun error en la conexion
    if (KERNEL->CXs[indice_celda].estado_cx == CLOSED) {
        KERNEL->num_CXs--;
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**) & KERNEL);
        return res;
    }
    
    desbloquear_acceso(&KERNEL->SEMAFORO);

    fprintf(stderr,"\nDevolvemos el kernel");
    //devolvemos el KERNEL
    ltm_exit_kernel((void**)&KERNEL);

    fprintf(stderr,"\nRetornamos a la aplicacion");
    return indice_celda;
}

int t_listen(t_direccion *tsap_escucha, t_direccion *tsap_remota) {
    int res = EXOK;

    fprintf(stderr,"\nEntramos al kernel");
    //entramos en el kernel
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
    if (er < 0)
        return EXKERNEL;

    fprintf(stderr,"\nComprobamos parametros del listen");
    // comprobar parametros metele ip local
    if (comprobar_parametros(tsap_remota, tsap_escucha, 'l') == -1) {
        fprintf(stderr, "\nTSAPS incorrecto");
        ltm_exit_kernel((void**) &KERNEL);
        return EXINVA;
    }
    
        pid_t pid = gettid();

    bloquear_acceso(&KERNEL->SEMAFORO);
    fprintf(stderr,"\nMiramos si hay espacio para una nueva conexion");
    //miramos si hay espacio para una nueva conexion
    if (KERNEL->num_CXs == NUM_MAX_CXs) {
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**) & KERNEL);
        return EXMAXC;
    }

    //miramos si no es un listen repetido
    if (buscar_listen_repetido(tsap_escucha, tsap_remota) >= 0) {
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**) & KERNEL);
        return EXCDUP;
    }



    fprintf(stderr,"\nBuscamos una celda libre");
    //buscamos celda libre, eliminamos de la lista de cx_libres, y anotamos el numero de conexiones
    int indice_celda = buscar_celda_libre();
    fprintf(stderr,"\nLa celda libre es: %d",indice_celda);
    KERNEL->CXs_libres.pop_front();
    KERNEL->num_CXs++;

    //bloquear acceso

    fprintf(stderr,"\nRellenamos los datos de la conexion");
    KERNEL->CXs[indice_celda].ap_pid = pid;
    KERNEL->CXs[indice_celda].estado_cx = LISTEN;
    KERNEL->CXs[indice_celda].celda_ocupada = 1;
    KERNEL->CXs[indice_celda].ip_local = tsap_escucha->ip;
    KERNEL->CXs[indice_celda].puerto_origen = tsap_escucha->puerto;
    if (tsap_remota == NULL) {
        fprintf(stderr,"\ntsap remota es null");
        KERNEL->CXs[indice_celda].ip_destino.s_addr = 0;
        KERNEL->CXs[indice_celda].puerto_destino = 0;
    } else {
        fprintf(stderr,"\ntsap remota no es null");
        KERNEL->CXs[indice_celda].ip_destino = tsap_remota->ip;
        KERNEL->CXs[indice_celda].puerto_destino = tsap_remota->puerto;
    }
    
    desbloquear_acceso(&KERNEL->SEMAFORO);

    fprintf(stderr,"\nNos dormimos a la espera de una conexion");
    //nos dormimos a la espera de conexion
    bloquea_llamada(&KERNEL->CXs[indice_celda].barC);
    fprintf(stderr,"\nNos despiertan, CONNECT recibido");

    bloquear_acceso(&KERNEL->SEMAFORO);
    res = KERNEL->CXs[indice_celda].resultado_peticion;

    fprintf(stderr,"\nComprobamos si hubo algun error en la conexion");
    //comprobamos si hubo algun error en la conexion
    if (KERNEL->CXs[indice_celda].estado_cx == LISTEN) {
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**) & KERNEL);
        return res;
    }

    desbloquear_acceso(&KERNEL->SEMAFORO);
    fprintf(stderr,"\nDevolvemos el kernel y retornamos a la aplicacion");
    ltm_exit_kernel((void**) & KERNEL);
    return indice_celda;
}


int t_disconnect(int id) {
    int res = EXOK;
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
    if (er < 0)
        return EXKERNEL;

    // ..... aqui vuestro codigo

    ltm_exit_kernel((void**) & KERNEL);
    return res;
}

size_t t_send(int id, const void *datos, size_t longitud, int8_t *flags) {
    int res = EXOK;
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);  
    if (er < 0)
        return EXKERNEL;

    // ..... aqui vuestro codigo

    ltm_exit_kernel((void**) & KERNEL);
    return res;
}

size_t t_receive(int id, void *datos, size_t longitud, int8_t *flags) {
    int res = EXOK;
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
    if (er < 0)
        return EXKERNEL;

    // ..... aqui vuestro codigo

    ltm_exit_kernel((void**) & KERNEL);
    return res;
}
