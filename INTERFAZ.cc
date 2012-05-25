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
    //inicia_barrera(&KERNEL->CXs[indice_celda].barC);

    KERNEL->CXs[indice_celda].ap_pid = pid;
    KERNEL->CXs[indice_celda].estado_cx = CONNECT;
    KERNEL->CXs[indice_celda].celda_ocupada= 1;
    //no estoy seguro de si necesito meter estos datos??
    KERNEL->CXs[indice_celda].ip_destino = tsap_destino->ip;
    KERNEL->CXs[indice_celda].ip_local = tsap_origen->ip;
    KERNEL->CXs[indice_celda].puerto_origen = tsap_origen->puerto;
    KERNEL->CXs[indice_celda].puerto_destino = tsap_destino->puerto;
    //desbloquear_acceso(&KERNEL->SEMAFORO);

    fprintf(stderr,"\ncreamos el pakete");
    //creamos el paquete
    //usamos tsap_destino_aux porque este ya no es const y asi crear_pkt ya no es const
    it_buffer = buscar_buffer_libre();
    it_buffer->contador_rtx = NUM_MAX_RTx;
    it_tx = KERNEL->CXs[indice_celda].TX.end();
    KERNEL->CXs[indice_celda].TX.splice(it_tx,KERNEL->buffers_libres,it_buffer);
    fprintf(stderr,"\nya hicimos el splice");
    it_tx = --KERNEL->CXs[indice_celda].TX.end();//"--" se hace para que apunte al ultimo elemento, y no al vacio
    fprintf(stderr,"\nit_tx->contador_rtx: %d",it_tx->contador_rtx);
    fprintf(stderr,"\nit_buffer->contador_rtx: %d",it_buffer->contador_rtx);

    memcpy(&tsap_destino_aux,tsap_destino,sizeof(t_direccion));
    fprintf(stderr,"\nvamos a llamar a crear_pkt");
    crear_pkt(it_tx->pkt,CR,&tsap_destino_aux,tsap_origen,NULL,0,indice_celda,0);
    fprintf(stderr,"\nEnviamos el pakete y nos bloqueamos");
    //enviamos el paquete y nos bloqueamos
    enviar_tpdu(tsap_destino->ip,it_tx->pkt,sizeof(tpdu));
    fprintf(stderr,"\nla id_local es: %d",indice_celda);
    desbloquear_acceso(&KERNEL->SEMAFORO);
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
    //KERNEL->CXs_libres.pop_front();
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
    
    //desbloquear_acceso(&KERNEL->SEMAFORO);

    fprintf(stderr,"\nNos dormimos a la espera de una conexion");
    
    //nos dormimos a la espera de conexion
    fprintf(stderr,"\ndormimos al listen de indice: %d\n",indice_celda);
    desbloquear_acceso(&KERNEL->SEMAFORO);
    bloquea_llamada(&KERNEL->CXs[indice_celda].barC);
    //bloquea_llamada(&KERNEL->barrera);
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
    int tamanho = MAX_DATOS; //inicializamos tamanho al máximo
    //int res = EXOK;
    
    //obtenemos el kernel
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);  
    if (er < 0)
        return EXKERNEL;
    
    char *puntero_datos = (char *)datos;

    //miramos si hay datos disponibles
    if(datos == NULL){
        ltm_exit_kernel((void**)&KERNEL);
        return EXNODATA;
    }
    
    //miramos si existe la conexion establecida
    bloquear_acceso(&KERNEL->SEMAFORO);
    if(KERNEL->CXs[id].estado_cx != ESTABLISHED){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXBADTID;
    }
    
    //miramos cuantos sends hacen falta
    int numero_sends;
    if(longitud <= MAX_DATOS){
        tamanho = longitud;
        numero_sends = 1;
    }else{
        tamanho = MAX_DATOS;
        numero_sends = longitud/MAX_DATOS;
        if(longitud%MAX_DATOS > 0){
            numero_sends += 1;
        }
    }
    //comenzamos la numeracion
    int datos_a_transmitir = longitud;
    int datos_enviados = 0;
    
    //rellenamos datos de los tsaps
    t_direccion tsap_destino, tsap_origen;
    tsap_origen.ip.s_addr = KERNEL->CXs[id].ip_local.s_addr;
    tsap_origen.puerto = KERNEL->CXs[id].puerto_origen;
    tsap_destino.ip.s_addr = KERNEL->CXs[id].ip_destino.s_addr;
    tsap_destino.puerto = KERNEL->CXs[id].puerto_destino;
    
    //definimos iteradores
    list<buf_pkt>:: iterator it_libres;
    list<buf_pkt>::iterator it_tx;
    
    //mientras queden datos por transmitir ... ENVIAMOS
    while(numero_sends > 0){
        //miramos si tenemos espacio en buffer de TX
        if((KERNEL->CXs[id].TX.size() < KERNEL->NUM_BUF_PKTS)){
            //buscamos un buffer_libre
            it_libres = buscar_buffer_libre();
            memcpy(it_libres->pkt->datos,puntero_datos,tamanho);
            puntero_datos = puntero_datos + tamanho;
            it_tx = KERNEL->CXs[id].TX.end();
            KERNEL->CXs[id].TX.splice(it_tx,KERNEL->buffers_libres,it_libres);
            it_tx = --KERNEL->CXs[id].TX.end();
            //creamos el pakete y lo enviamos
            crear_pkt(it_tx->pkt,DATOS,&tsap_destino,&tsap_origen,it_tx->pkt->datos,tamanho,id,KERNEL->CXs[id].id_destino);
            enviar_tpdu(tsap_destino.ip,it_tx->pkt,sizeof(tpdu));
            it_tx->estado_pkt = no_confirmado;
            it_tx->contador_rtx = NUM_MAX_RTx;
            it_tx->num_secuencia = KERNEL->CXs[id].numero_secuencia;
            KERNEL->CXs[id].numero_secuencia++;
            numero_sends--;
            datos_a_transmitir-=tamanho;//tamanho vale MAX_DATOS
            datos_enviados+=tamanho;
            if(numero_sends == 1){
                //ULTIMO PKT-> actualizamos tamanho
                tamanho = datos_a_transmitir;
            }
        }else{//si no me duermo
            KERNEL->CXs[id].primitiva_dormida = true;
            desbloquear_acceso(&KERNEL->SEMAFORO);
            bloquea_llamada(&KERNEL->CXs[id].barC);
            bloquear_acceso(&KERNEL->SEMAFORO);
            KERNEL->CXs[id].primitiva_dormida = false;
            
            if(KERNEL->CXs[id].resultado_primitiva == EXNET){
                desbloquear_acceso(&KERNEL->SEMAFORO);
                ltm_exit_kernel((void**)&KERNEL);
                return EXNET;
            }
        }
    }
    
    desbloquear_acceso(&KERNEL->SEMAFORO);
    
    // ..... aqui vuestro codigo

    ltm_exit_kernel((void**) & KERNEL);
    return datos_enviados;
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
