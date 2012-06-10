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
    list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_buffer;
    list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_tx;
    list<evento_t, shm_Allocator<evento_t> >::iterator it_nuevo_tempo;
    list<evento_t, shm_Allocator<evento_t> >::iterator it_temp;
#ifdef DEPURA
    //fprintf(stderr,"\nObtenemos el Kernel");
#endif
    //entrar en KERNEL
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
    if (er < 0)
        return EXKERNEL;
    
    bloquear_acceso(&KERNEL->SEMAFORO);
    
    //fprintf(stderr,"\nComprobamos parametros");
    // comprobar parametros
    if (comprobar_parametros(tsap_destino,tsap_origen,'c') == -1) {
        //fprintf(stderr,"\nTSAPS incorrecto");
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**) &KERNEL);
        return EXINVA;
    }
    
    pid_t pid = gettid();

    //bloquear_acceso(&KERNEL->SEMAFORO);

    //fprintf(stderr,"\nMiramos si hay espacio para una nueva conexion");
    //miramos si hay espacio para una nueva conexion
    if(KERNEL->num_CXs == NUM_MAX_CXs){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXMAXC;
    }

    //fprintf(stderr,"\nMiramos si el connect no esta repetido");
    if(buscar_connect_repetido(tsap_origen,tsap_destino)>-1){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXCDUP;
    }

    //fprintf(stderr,"\nBuscamos celda libre");
    //localizar celda libre, elimino la conexion libre y aumento el numero de conexiones.
    int indice_celda = buscar_celda_libre();
    KERNEL->CXs_libres.pop_front();
    KERNEL->num_CXs++;

    
    //int indice_celda = KERNEL->indice_libre;//siempre tendremos el indice libre mas bajo

    //fprintf(stderr,"\nRellenamos los datos de la conexion");
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

    //fprintf(stderr,"\ncreamos el pakete");
    //creamos el paquete
    //usamos tsap_destino_aux porque este ya no es const y asi crear_pkt ya no es const
    it_buffer = buscar_buffer_libre();
    it_buffer->contador_rtx = NUM_MAX_RTx;
    it_tx = KERNEL->CXs[indice_celda].TX.end();
    KERNEL->CXs[indice_celda].TX.splice(it_tx,KERNEL->buffers_libres,it_buffer);
    //fprintf(stderr,"\nya hicimos el splice");
    it_tx = --KERNEL->CXs[indice_celda].TX.end();//"--" se hace para que apunte al ultimo elemento, y no al vacio
    //it_tx->pkt = (tpdu *)(it_tx->contenedor);
    //fprintf(stderr,"\nit_tx->contador_rtx: %d",it_tx->contador_rtx);
    //fprintf(stderr,"\nit_buffer->contador_rtx: %d",it_buffer->contador_rtx);

    memcpy(&tsap_destino_aux,tsap_destino,sizeof(t_direccion));
    //fprintf(stderr,"\nvamos a llamar a crear_pkt");
    crear_pkt(it_tx->pkt,CR,&tsap_destino_aux,tsap_origen,NULL,0,indice_celda,0);
    //fprintf(stderr,"\nEnviamos el pakete y nos bloqueamos");
    //fprintf(stderr,"\nit_tx->pkt->cabecera.num_secuencia: %d",it_tx->pkt->cabecera.numero_secuencia);
    //pruebas
    it_tx = --KERNEL->CXs[indice_celda].TX.end();
    //it_tx->pkt=(tpdu *)(it_tx->contenedor);
    //fprintf(stderr,"\nvolvemos al it_tx-> it_tx->pkt->cabecera.num_secuencia: %d",it_tx->pkt->cabecera.numero_secuencia);
    //enviamos el paquete y nos bloqueamos
    enviar_tpdu(tsap_destino->ip,it_tx->pkt,sizeof(tpdu));

    //creamos el temporizador para el CR
    it_nuevo_tempo = buscar_temporizador_libre();
    it_nuevo_tempo->indice_cx = indice_celda;
    it_nuevo_tempo->timeout = tiempo_rtx_pkt;
    it_nuevo_tempo->tipo_tempo = vencimiento_pkt;
    //hacemos que apunte el temporizador->buffer y buffer->temporizador
    it_nuevo_tempo->it_pkt = it_tx;
    it_tx->it_tout_pkt = it_nuevo_tempo;
    //pasamos el temporizador a la lista de tempo_pkts
    it_temp = KERNEL->tout_pkts.end();
    KERNEL->tout_pkts.splice(it_temp,KERNEL->temporizadores_libres,it_nuevo_tempo);

    //fprintf(stderr,"\nla id_local es: %d",indice_celda);
    desbloquear_acceso(&KERNEL->SEMAFORO);
    bloquea_llamada(&KERNEL->CXs[indice_celda].barC);
    
    bloquear_acceso(&KERNEL->SEMAFORO);//bloqueamos para leer de kernel
    res = KERNEL->CXs[indice_celda].resultado_peticion;

    //fprintf(stderr,"\nComprobamos si hubo algun error en la conexion");
    //comprobamos si hubo algun error en la conexion
    if (KERNEL->CXs[indice_celda].estado_cx == CLOSED) {
        KERNEL->num_CXs--;
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**) & KERNEL);
        return res;
    }

    //iniciamos el temporizador de red y el de aplicacion
    
    //temporizador de red
    it_nuevo_tempo = buscar_temporizador_libre();
    it_nuevo_tempo->indice_cx = indice_celda;
    it_nuevo_tempo->timeout = tiempo_rtx_red;
    it_nuevo_tempo->tipo_tempo = vencimiento_red;
    //enlazamos la conexion a su temporizador de red
    KERNEL->CXs[indice_celda].it_tempo_red = it_nuevo_tempo;
    it_temp = KERNEL->tout_red_aplic.end();
    KERNEL->tout_red_aplic.splice(it_temp,KERNEL->temporizadores_libres,it_nuevo_tempo);

    //temporizador de aplicacion
    it_nuevo_tempo = buscar_temporizador_libre();
    it_nuevo_tempo->indice_cx = indice_celda;
    it_nuevo_tempo->timeout = tiempo_rtx_aplic;
    it_nuevo_tempo->tipo_tempo = vencimiento_aplic;
    //enlazamos la conexion a su temporizador de aplicacion
    KERNEL->CXs[indice_celda].it_tempo_aplic = it_nuevo_tempo;
    it_temp = KERNEL->tout_red_aplic.end();
    KERNEL->tout_red_aplic.splice(it_temp,KERNEL->temporizadores_libres,it_nuevo_tempo);

    desbloquear_acceso(&KERNEL->SEMAFORO);

    //fprintf(stderr,"\nDevolvemos el kernel");
    //devolvemos el KERNEL
    ltm_exit_kernel((void**)&KERNEL);

    //fprintf(stderr,"\nRetornamos a la aplicacion");
    return indice_celda;
}

int t_listen(t_direccion *tsap_escucha, t_direccion *tsap_remota) {
    int res = EXOK;

    //fprintf(stderr,"\nEntramos al kernel");
    //entramos en el kernel
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
    if (er < 0)
        return EXKERNEL;

    bloquear_acceso(&KERNEL->SEMAFORO);
    //fprintf(stderr,"\nComprobamos parametros del listen");
    // comprobar parametros metele ip local
    if (comprobar_parametros(tsap_remota, tsap_escucha, 'l') == -1) {
        //fprintf(stderr, "\nTSAPS incorrecto");
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**) &KERNEL);
        return EXINVA;
    }
    
        pid_t pid = gettid();

    //bloquear_acceso(&KERNEL->SEMAFORO);
    //fprintf(stderr,"\nMiramos si hay espacio para una nueva conexion");
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



    //fprintf(stderr,"\nBuscamos una celda libre");
    //buscamos celda libre, eliminamos de la lista de cx_libres, y anotamos el numero de conexiones
    int indice_celda = buscar_celda_libre();
    //fprintf(stderr,"\nLa celda libre es: %d",indice_celda);
    //KERNEL->CXs_libres.pop_front();
    KERNEL->num_CXs++;

    //bloquear acceso

    //fprintf(stderr,"\nRellenamos los datos de la conexion");
    KERNEL->CXs[indice_celda].ap_pid = pid;
    KERNEL->CXs[indice_celda].estado_cx = LISTEN;
    KERNEL->CXs[indice_celda].celda_ocupada = 1;
    KERNEL->CXs[indice_celda].ip_local = tsap_escucha->ip;
    KERNEL->CXs[indice_celda].puerto_origen = tsap_escucha->puerto;
    if (tsap_remota == NULL) {
       // fprintf(stderr,"\ntsap remota es null");
        KERNEL->CXs[indice_celda].ip_destino.s_addr = 0;
        KERNEL->CXs[indice_celda].puerto_destino = 0;
    } else {
        //fprintf(stderr,"\ntsap remota no es null");
        KERNEL->CXs[indice_celda].ip_destino = tsap_remota->ip;
        KERNEL->CXs[indice_celda].puerto_destino = tsap_remota->puerto;
    }
    
    //desbloquear_acceso(&KERNEL->SEMAFORO);

    //fprintf(stderr,"\nNos dormimos a la espera de una conexion");
    
    //nos dormimos a la espera de conexion
    //fprintf(stderr,"\ndormimos al listen de indice: %d\n",indice_celda);
    desbloquear_acceso(&KERNEL->SEMAFORO);
    bloquea_llamada(&KERNEL->CXs[indice_celda].barC);
    //bloquea_llamada(&KERNEL->barrera);
    //fprintf(stderr,"\nNos despiertan, CONNECT recibido");

    bloquear_acceso(&KERNEL->SEMAFORO);
    res = KERNEL->CXs[indice_celda].resultado_peticion;

    //fprintf(stderr,"\nComprobamos si hubo algun error en la conexion");
    //comprobamos si hubo algun error en la conexion
    if (KERNEL->CXs[indice_celda].estado_cx == LISTEN) {
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**) & KERNEL);
        return res;
    }

    //iniciamos el temporizador de red y el de aplicacion
    list<evento_t, shm_Allocator<evento_t> >::iterator it_temp;
    //temporizador de red
    list<evento_t, shm_Allocator<evento_t> >::iterator it_nuevo_tempo = buscar_temporizador_libre();
    it_nuevo_tempo->indice_cx = indice_celda;
    it_nuevo_tempo->timeout = tiempo_rtx_red;
    it_nuevo_tempo->tipo_tempo = vencimiento_red;
    it_temp = KERNEL->tout_red_aplic.end();
    KERNEL->tout_red_aplic.splice(it_temp,KERNEL->temporizadores_libres,it_nuevo_tempo);
    //apuntamos el temporizador de red en la conexion
    KERNEL->CXs[indice_celda].it_tempo_red = --KERNEL->tout_red_aplic.end();
    
    //temporizador de aplicacion
    it_nuevo_tempo = buscar_temporizador_libre();
    it_nuevo_tempo->indice_cx = indice_celda;
    it_nuevo_tempo->timeout = tiempo_rtx_aplic;
    it_nuevo_tempo->tipo_tempo = vencimiento_aplic;
    it_temp = KERNEL->tout_red_aplic.end();
    KERNEL->tout_red_aplic.splice(it_temp,KERNEL->temporizadores_libres,it_nuevo_tempo);
    //apuntamos el temporizador de aplicacion en la conexion
    KERNEL->CXs[indice_celda].it_tempo_aplic = --KERNEL->tout_red_aplic.end();

    

    desbloquear_acceso(&KERNEL->SEMAFORO);
    //fprintf(stderr,"\nDevolvemos el kernel y retornamos a la aplicacion");
    ltm_exit_kernel((void**) & KERNEL);
    return indice_celda;
}


int t_disconnect(int id) {
    list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_libre;
    list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_tx;

    list<evento_t, shm_Allocator<evento_t> >::iterator it_nuevo_tempo;
    list<evento_t, shm_Allocator<evento_t> >::iterator it_temp;
    
    //obtenemos el KERNEL
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
    if (er < 0)
        return EXKERNEL;
    
    //miramos si la cx existe en la tabla
    bloquear_acceso(&KERNEL->SEMAFORO);
    if(KERNEL->CXs[id].estado_cx == CLOSED){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXBADTID;
    }

    //actualizamos el temporizador de la aplicacion
    recalcular_temporizador_aplic(id);
    
    //rellenamos datos de los TSAPs
    t_direccion tsap_destino, tsap_origen;
    tsap_origen.ip.s_addr = KERNEL->CXs[id].ip_local.s_addr;
    tsap_origen.puerto = KERNEL->CXs[id].puerto_origen;
    tsap_destino.ip.s_addr = KERNEL->CXs[id].ip_destino.s_addr;
    tsap_destino.puerto = KERNEL->CXs[id].puerto_destino;
    
    it_libre = buscar_buffer_libre();
    it_tx = KERNEL->CXs[id].TX.end();
    KERNEL->CXs[id].TX.splice(it_tx,KERNEL->buffers_libres,it_libre);
    it_tx = --KERNEL->CXs[id].TX.end();
    it_tx->contador_rtx = NUM_MAX_RTx;
    crear_pkt(it_tx->pkt, DR, &tsap_destino, &tsap_origen, NULL, 0, id, KERNEL->CXs[id].id_destino);
    //miramos si podemos enviarlo
    if(KERNEL->CXs[id].TX.size() == 1){
        enviar_tpdu(tsap_destino.ip, it_tx->pkt, sizeof(tpdu));
    }

    //creamos el temporizador para el PKT_DR
    it_nuevo_tempo = buscar_temporizador_libre();
    it_nuevo_tempo->indice_cx = id;
    it_nuevo_tempo->timeout = tiempo_rtx_pkt;
    it_nuevo_tempo->tipo_tempo = vencimiento_pkt;
    //hacemos que apunte el temporizador->buffer y buffer->temporizador
    it_nuevo_tempo->it_pkt = it_tx;
    it_tx->it_tout_pkt = it_nuevo_tempo;
    //pasamos el temporizador a la lista de tempo_pkts
    it_temp = KERNEL->tout_pkts.end();
    KERNEL->tout_pkts.splice(it_temp, KERNEL->temporizadores_libres, it_nuevo_tempo);
    
    
    // ..... aqui vuestro codigo
    desbloquear_acceso(&KERNEL->SEMAFORO);
    ltm_exit_kernel((void**) & KERNEL);
    return EXOK;
}

size_t t_send(int id, const void *datos, size_t longitud, int8_t *flags) {
    int tamanho = MAX_DATOS; //inicializamos tamanho al máximo
    list<evento_t, shm_Allocator<evento_t> >::iterator it_nuevo_tempo;
    list<evento_t, shm_Allocator<evento_t> >::iterator it_temp;
    //int res = EXOK;
    //fprintf(stderr,"\nsend de longitud: %d",longitud);
    //obtenemos el kernel
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);  
    if (er < 0)
        return EXKERNEL;
    
    bloquear_acceso(&KERNEL->SEMAFORO);
    
    char *puntero_datos = (char *)datos;

    //miramos si hay datos disponibles
    if(datos == NULL){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXNODATA;
    }
    
    //miramos si existe la conexion establecida
    //bloquear_acceso(&KERNEL->SEMAFORO);
    if(KERNEL->CXs[id].estado_cx != ESTABLISHED){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXBADTID;
    }

    if (KERNEL->CXs[id].close_aplicacion) {
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**) &KERNEL);
        return EXCLOSE;
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
    list<buf_pkt, shm_Allocator<buf_pkt> >:: iterator it_libres;
    list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_tx;

    //actualizamos el temporizador de la aplicacion
    recalcular_temporizador_aplic(id);
    
    //mientras queden datos por transmitir ... ENVIAMOS
    while(numero_sends > 0){

        //miramos si la conexion origen no se esta cerrando, 
        //debido a activacion del signal_disconnect por un DISCONNECT
        if (KERNEL->CXs[id].signal_disconnect == true) {
            desbloquear_acceso(&KERNEL->SEMAFORO);
            ltm_exit_kernel((void**) &KERNEL);
            return EXDISC;
        }
        
        //miramos si la TSAP remoto no ha cerrado su conexion
//        if(KERNEL->CXs[id].desconexion_remota){
//            desbloquear_acceso(&KERNEL->SEMAFORO);
//            ltm_exit_kernel((void**)&KERNEL);
//            return EXCLOSE;
//        }
        
        //miramos si tenemos espacio en buffer de TX
        if((KERNEL->CXs[id].TX.size() < KERNEL->NUM_BUF_PKTS)) {
            
           // fprintf(stderr,"\nSEND: hay espacio en el buffer de TX");
            //buscamos un buffer_libre
            it_libres = buscar_buffer_libre();
            //it_libres->pkt = (tpdu *)(it_libres->contenedor);
            //memcpy(it_libres->pkt->datos,puntero_datos,tamanho);
            //fprintf(stderr,"\nSEND: copiamos los datos a it_libres");
            //puntero_datos = puntero_datos + tamanho;
            it_tx = KERNEL->CXs[id].TX.end();
            KERNEL->CXs[id].TX.splice(it_tx,KERNEL->buffers_libres,it_libres);
            //fprintf(stderr,"\nSEND: pasamos el buffer a la lista de TX");
            it_tx = --KERNEL->CXs[id].TX.end();
            it_tx->pkt = (tpdu *)(it_tx->contenedor);
            it_tx->contador_rtx = NUM_MAX_RTx;
            //creamos el pakete y lo enviamos, crear_pkt pone cabecera.close=0 por defecto
            crear_pkt(it_tx->pkt,DATOS,&tsap_destino,&tsap_origen,puntero_datos,tamanho,id,KERNEL->CXs[id].id_destino);
            //fprintf(stderr,"\nSEND: creado el pakete de DATOS");
                        //si es el ultimo PKT miramos si FLAGS = SEND_CLOSE
            if((numero_sends == 1)&&(((*flags)&CLOSE) == CLOSE) ){
               // fprintf(stderr,"\nSEND: es el ultimo pakete con MODO CLOSE");
                it_tx->pkt->cabecera.close = 1;// solo lo pongo a uno en el ultimo pakete
                //creo que deberia poner signal_disconnect=true
                KERNEL->CXs[id].close_aplicacion = true;
                //avisamos a la aplicacion del envio de todos los datos
                //*flags=*flags & (0xFF^CLOSE);
            }
            
            //fprintf(stderr, "\nit_tx->pkt->cabecera.close: %d", it_tx->pkt->cabecera.close);
            //fprintf(stderr, "\nit_tx->pkt->cabecera.tipo: %d", it_tx->pkt->cabecera.tipo);
            //fprintf(stderr, "\nit_tx->pkt->cabecera.id_local: %d", it_tx->pkt->cabecera.id_local);
            //fprintf(stderr, "\nit_tx->pkt->cabecera.id_destino: %d", it_tx->pkt->cabecera.id_destino);
            //fprintf(stderr, "\nit_tx->pkt->cabecera.puerto_orig: %d", it_tx->pkt->cabecera.puerto_orig);
            //fprintf(stderr, "\nit_tx->pkt->cabecera.puerto_dest: %d", it_tx->pkt->cabecera.puerto_dest);
            //fprintf(stderr, "\nit_tx->pkt->cabecera.numero_secuencia: %d", it_tx->pkt->cabecera.numero_secuencia);


            enviar_tpdu(tsap_destino.ip,it_tx->pkt,sizeof(tpdu));

            //creamos el temporizador para el PKT_DATOS
            it_nuevo_tempo = buscar_temporizador_libre();
            it_nuevo_tempo->indice_cx = id;
            it_nuevo_tempo->timeout = tiempo_rtx_pkt;
            //fprintf(stderr,"\nit_nuevo_tempo->timeout: %d", it_nuevo_tempo->timeout);
            it_nuevo_tempo->tipo_tempo = vencimiento_pkt;
            //hacemos que apunte el temporizador->buffer y buffer->temporizador
            it_nuevo_tempo->it_pkt = it_tx;
            it_tx->it_tout_pkt = it_nuevo_tempo;
            //pasamos el temporizador a la lista de tempo_pkts
            it_temp = KERNEL->tout_pkts.end();
            KERNEL->tout_pkts.splice(it_temp, KERNEL->temporizadores_libres, it_nuevo_tempo);

            //fprintf(stderr,"\nSEND: enviado TDPU");

            //actualizamos el shortest provocando un interrumpe_daemon
            if(KERNEL->tout_pkts.size() == 1){
                interrumpe_daemon();
            }
               
            it_tx->estado_pkt = no_confirmado;
            it_tx->contador_rtx = NUM_MAX_RTx;
            it_tx->num_secuencia = KERNEL->CXs[id].numero_secuencia;
            KERNEL->CXs[id].numero_secuencia++;
            numero_sends--;
            puntero_datos = puntero_datos+tamanho;
            datos_a_transmitir-=tamanho;//tamanho vale MAX_DATOS
            datos_enviados+=tamanho;            
            if(numero_sends == 1){
                //ULTIMO PKT-> actualizamos tamanho
                tamanho = datos_a_transmitir;
            }
        }else{//si no me duermo
            //fprintf(stderr,"\nSEND: no hay espacio en lista de TX, me duermo");
            KERNEL->CXs[id].primitiva_dormida = true;
            desbloquear_acceso(&KERNEL->SEMAFORO);
            bloquea_llamada(&KERNEL->CXs[id].barC);
            bloquear_acceso(&KERNEL->SEMAFORO);
            //fprintf(stderr,"\nSEND: BPRINCIPAL me despierta, ya hay sitio en lista de TX");
            //KERNEL->CXs[id].primitiva_dormida = false;
            //miramos por que me despertaron y si hubo error
            if(KERNEL->CXs[id].resultado_primitiva == EXNET){
                desbloquear_acceso(&KERNEL->SEMAFORO);
                ltm_exit_kernel((void**)&KERNEL);
                return EXNET;
            }
        }
    }

    if (KERNEL->CXs[id].close_remoto) {
        *flags = (*flags)||CLOSE;
    }else{
        *flags = (*flags)&&(!CLOSE);
    }
    
    desbloquear_acceso(&KERNEL->SEMAFORO);
    
    // ..... aqui vuestro codigo
    //fprintf(stderr,"\nSEND: primitiva finalizada");
    ltm_exit_kernel((void**) & KERNEL);
    //fprintf(stderr,"\nSEND: return datos_enviados: %d",datos_enviados);

    return datos_enviados;
}

size_t t_receive(int id, void *datos, size_t longitud, int8_t *flags) {
    char *datos_aux =(char *) datos;
    //fprintf(stderr,"\nRECEIVE: receive de longitud: %d",longitud);
    //fprintf(stderr,"\nRECEIVE: obtenemos el kernel");
    //obtenemos el KERNEL
    int er = ltm_get_kernel(dir_proto, (void**) & KERNEL);
    if (er < 0)
        return EXKERNEL;
    
    bloquear_acceso(&KERNEL->SEMAFORO);
    
    //fprintf(stderr,"\nRECEIVE: miramos si datos==NULL");
    //miramos si hay datos disponibles
    if((datos == NULL)||(longitud < 0)){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXINVA;
    }
    
    //fprintf(stderr,"\nRECEIVE: miramos si la conexion esta ESTABLISHED");
    //miramos si existe la conexion establecida
    if(KERNEL->CXs[id].estado_cx != ESTABLISHED){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXBADTID;
    }
    
    //fprintf(stderr,"\nRECEIVE: miramos si la entidad remota me mando un SEND_CLOSE");
    //miramos si recibimos peticion de desconexion, y si no hay nada que entregar
    if(KERNEL->CXs[id].close_remoto == true){ //&& KERNEL->CXs[id].RX.size() == 0){
        desbloquear_acceso(&KERNEL->SEMAFORO);
        ltm_exit_kernel((void**)&KERNEL);
        return EXCLOSE;
    }

    //miramos cuanto tenemos que recibir en el bucle
    int datos_por_recibir = longitud;
    int datos_recibidos = 0;
    
    //definimos iteradores
    list<buf_pkt, shm_Allocator<buf_pkt> >:: iterator it_libres;
    list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_rx;
    
    unsigned int indice;
    unsigned int num_buf_rx;

    int flag_fin_primitiva = 0;

    //recalculamos el temporizador de aplicacion
    recalcular_temporizador_aplic(id);

    //fprintf(stderr,"\nRECEIVE: Empezamos a recibir");
    //nos disponemos a recibir
    while(datos_por_recibir > 0){
        if(!(KERNEL->CXs[id].RX.empty())){//si hay datos en buffer RX ...
           // fprintf(stderr,"\nRECEIVE: el buffer de RX no esta vacio, tamanho: %d",KERNEL->CXs[id].RX.size());
            num_buf_rx = KERNEL->CXs[id].RX.size();//lo hacemos porque si no variaria el size al hacer un splice
            for(indice=0; indice < num_buf_rx;indice++){
                //fprintf(stderr,"\nRECEIVE: miramos si la entidad remota hizo un DISCONNECT");
                //si la entidad remota hizo un DISCONNECT abrupto damos el error EXDISC
                if(KERNEL->CXs[id].signal_disconnect == true) {
                    desbloquear_acceso(&KERNEL->SEMAFORO);
                    ltm_exit_kernel((void**) &KERNEL);
                    return EXDISC;
                }
                
                it_rx = KERNEL->CXs[id].RX.begin();
                it_rx->pkt = (tpdu *)(it_rx->contenedor+20);
                //fprintf(stderr,"\nit_rx->pkt->cabecera.close: %d",it_rx->pkt->cabecera.close);
                //fprintf(stderr,"\nit_rx->pkt->cabecera.tipo: %d",it_rx->pkt->cabecera.tipo);
                //fprintf(stderr,"\nit_rx->pkt->cabecera.id_local: %d",it_rx->pkt->cabecera.id_local);
                //fprintf(stderr,"\nit_rx->pkt->cabecera.id_destino: %d",it_rx->pkt->cabecera.id_destino);
                //fprintf(stderr,"\nit_rx->pkt->cabecera.puerto_orig: %d",it_rx->pkt->cabecera.puerto_orig);
                //fprintf(stderr,"\nit_rx->pkt->cabecera.puerto_dest: %d",it_rx->pkt->cabecera.puerto_dest);
                //fprintf(stderr, "\nit_rx->pkt->cabecera.numero_secuencia: %d", it_rx->pkt->cabecera.numero_secuencia);


//                //fprintf(stderr,"\nRECEIVE: Miramos si cabecera.close=1");
//                //miramos si es el ultimo pakete con CLOSE para avisar a la aplicacion
//                if (it_rx->pkt->cabecera.close == 1) {
//                    //fprintf(stderr,"\nRECEIVE: casca al comprobar la cabecera.close=1");
//                    *flags = (*flags || CLOSE);
//                    flag_fin_primitiva = 1;
//                    
//                }else{
//                    //fprintf(stderr,"\nRECEIVE: la cabecera->close != 1");
//                }

                //fprintf(stderr,"\nRECEIVE: datos_aux: %s",&(*datos_aux));
                //VERSION NUEVA!!!
                if(it_rx->bytes_restan > longitud){//leemos lo que podamos del buffer de rx
                    memcpy(datos_aux,it_rx->ultimo_byte, longitud);
                    //fprintf(stderr,"\nRECEIVE: bytes_restan>longitud -> copiados los datos del buffer");
                    it_rx->bytes_restan -= longitud;
                    it_rx->ultimo_byte+=longitud;
                    datos_aux+=longitud;
                    datos_recibidos+=longitud;
                    datos_por_recibir-=longitud;
                }else{//leemos todo el buffer de rx
                    memcpy(datos_aux,it_rx->ultimo_byte,it_rx->bytes_restan);
                    //fprintf(stderr,"\nRECEIVE: bytes_restan<=longitud->copiados los datos del buffer");
                    //fprintf(stderr,"\nRECEIVE: datos_por_recibir: %d",datos_por_recibir);
                    //fprintf(stderr,"\nRECEIVE: it_rx->bytes_restan: %d",it_rx->bytes_restan);
                    //fprintf(stderr,"\nRECEIVE: datos_recibidos: %d",datos_recibidos);
                    datos_por_recibir -= it_rx->bytes_restan;
                    datos_recibidos += it_rx->bytes_restan;
                    datos_aux+=it_rx->bytes_restan;
                    it_rx->ultimo_byte+=it_rx->bytes_restan;
                    it_rx->bytes_restan -= it_rx->bytes_restan;

                    //fprintf(stderr,"\nRECEIVE: Miramos si cabecera.close=1");
                    //miramos si es el ultimo pakete con CLOSE para avisar a la aplicacion
                    if (it_rx->pkt->cabecera.close == 1) {
                        //fprintf(stderr,"\nRECEIVE: casca al comprobar la cabecera.close=1");
                        //*flags = (*flags || CLOSE);
                        KERNEL->CXs[id].close_remoto = true;

                    } else {
                        //fprintf(stderr,"\nRECEIVE: la cabecera->close != 1");
                    }
                    
                    //pasamos el buffer de rx a libres
                    it_libres = KERNEL->buffers_libres.end();
                    KERNEL->buffers_libres.splice(it_libres,KERNEL->CXs[id].RX,it_rx);
                    break;
                }
            }
        }else{//si no hay datos en buffer RX-> DORMIRSE
            //fprintf(stderr, "RECEIVE: el buffer de recepcion esta vacio, nos dormimos");
            KERNEL->CXs[id].primitiva_dormida = true;
            desbloquear_acceso(&KERNEL->SEMAFORO);
            bloquea_llamada(&KERNEL->CXs[id].barC);
            bloquear_acceso(&KERNEL->SEMAFORO);
            //KERNEL->CXs[id].primitiva_dormida = false;
        }
        if(flag_fin_primitiva == 1){
            break;
        }
    }
    
    if(KERNEL->CXs[id].close_remoto){
        *flags = (*flags)||CLOSE;
    }else{
        *flags = (*flags)&&(!CLOSE);
    }
    // ..... aqui vuestro codigo
    desbloquear_acceso(&KERNEL->SEMAFORO);
    ltm_exit_kernel((void**) & KERNEL);
    return datos_recibidos;
}
