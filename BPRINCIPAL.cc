#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "ltmdaemon.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/time.h>

#include "CONFP.h"

///////////////////////////////////////////////////////////////////////////

void bucle_principal(void) {
    bool daemon_stop = false;

    //char pkt[1000]; ESTO LO QUITE YO PARA USAR MAX_LONG_PKT
    unsigned int offset=0;
    int cont = 0;
    int shortest = -1;
    struct in_addr ip_remota;
    char ipcharbuf[20];
    int j;
    
        
    //datos mios
    char pkt[MAX_LONG_PKT];
    tpdu *puntero_pkt;
    tpdu paquete;//esto me vale de momento->> luego hacer malloc(sizeof(tpdu))
    t_direccion tsap_origen,tsap_destino;//no se si usarlas aqui o meterlas en la cabecera tcp
    //int iIdCx = 0; //cuando inicia ltmd iIdCx = 0, con cada CONNECT iIcCx++

    //Iniciamos el semaforo
    inicia_semaforo(&KERNEL->SEMAFORO);

    //Inicializamos la memoria compartida
    bloquear_acceso(&KERNEL->SEMAFORO);
    KERNEL->num_CXs=0;
    KERNEL->indice_libre =0;
    KERNEL->NUM_BUF_PKTS = NUM_BUF_PKTS;
    int i;
    int indice;
    //inicializamos cada conexion
    for(i=0;i<NUM_MAX_CXs;i++){
        //memset(&KERNEL->CXs[i],0,sizeof(conexion_t));
        fprintf(stderr,"\ninicializo barrera: %d",i);
        inicia_barrera(&KERNEL->CXs[i].barC);
        
        KERNEL->CXs[i].estado_cx = CLOSED;
        KERNEL->CXs[i].signal_disconnect = false;
        KERNEL->CXs[i].desconexion_remota = false;
        INICIA_LISTA(KERNEL->CXs[i].TX,buf_pkt);
        INICIA_LISTA(KERNEL->CXs[i].RX,buf_pkt);
    }
    inicia_barrera(&KERNEL->barrera);

    INICIA_LISTA(KERNEL->buffers_libres, buf_pkt);
    //Inicializamos la lista de conexiones libres
    INICIA_LISTA(KERNEL->CXs_libres, int);
    inicializar_CXs_libres();
    
    struct timeval tv;
    gettimeofday(&tv,NULL);
    KERNEL->t_inicio = tv.tv_sec*1000+tv.tv_usec/1000;
    
    desbloquear_acceso(&KERNEL->SEMAFORO);

    do {

        switch (ltm_wait4event(shortest)) {

            case TIME_OUT:
                fprintf(stderr, "El protocolo despierta por TMOUT (hora %ld)\n", time(0));

                break;

            case INTERRUP:
                break;

            case PAQUETE:                
                fprintf(stderr,"\n (%d) Se ha recibido un PAQUETE ", cont + 1);
                                
                list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_buffer;
                list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_libres;
                list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_tx;
                list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_rx;
                int resul;
                
                it_libres = buscar_buffer_libre();
                
                //int rec = recibir_tpdu(pkt, MAX_LONG_PKT, &ip_remota, &offset);
                int rec = recibir_tpdu(&it_libres->pkt, MAX_LONG_PKT, &ip_remota, &offset);
                fprintf(stderr,"\nrec: %d",rec);
                if (rec >= 0) {
                    fprintf(stderr,"desde la IP %s\ntexto del mensaje: %s\n",
                            //inet_ntop(AF_INET, &ip_remota, ipcharbuf, 20), pkt + offset);
                            inet_ntop(AF_INET, &ip_remota, ipcharbuf, 20), &(it_libres->pkt) + offset);
                }
                
                fprintf(stderr,"\nHola!!!!");
                //puntero_pkt = (tpdu *)(pkt+offset)
                puntero_pkt = (tpdu *)(&(it_libres->pkt)+offset);
                fprintf(stderr,"\nHola2!!!!");
                
                bloquear_acceso(&KERNEL->SEMAFORO);

                
                switch(puntero_pkt->cabecera.tipo){
                    
                    case CC:
                        fprintf(stderr,"\nRecibido un CONEXION CONFIRMED");
                         //si el cc es negativo CLOSED, si no conexion ESTABLISHED
                        if(puntero_pkt->cabecera.conexion_aceptada < 0){
                            fprintf(stderr,"\nrecibido CCNegativo CLOSED");
                            KERNEL->CXs[puntero_pkt->cabecera.id_destino].estado_cx = CLOSED;
                            KERNEL->CXs[puntero_pkt->cabecera.id_destino].celda_ocupada = 0;
                            KERNEL->CXs[puntero_pkt->cabecera.id_destino].resultado_peticion=puntero_pkt->cabecera.conexion_aceptada;
                        } else {
                            fprintf(stderr,"\nRecibido CCPositivo ESTABLISHED");
                            KERNEL->CXs[puntero_pkt->cabecera.id_destino].estado_cx = ESTABLISHED;
                            KERNEL->CXs[puntero_pkt->cabecera.id_destino].id_destino = puntero_pkt->cabecera.id_local;
                            KERNEL->CXs[puntero_pkt->cabecera.id_destino].resultado_peticion=EXOK;
                            //kitamos del buffer de TX el pkt CR
                            it_libres = KERNEL->buffers_libres.end();
                            it_tx = --KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX.end();
                            KERNEL->buffers_libres.splice(it_libres,KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX,it_tx);
                            //iniciamos el NUMERO DE SECUENCIA para esta conexion
                            KERNEL->CXs[puntero_pkt->cabecera.id_destino].numero_secuencia = 0;
                        }
                        desbloquear_acceso(&KERNEL->SEMAFORO);
                        fprintf(stderr,"\nid_estino es: %d",puntero_pkt->cabecera.id_local);
                        fprintf(stderr,"\nid_local es: %d",puntero_pkt->cabecera.id_destino);
                        despierta_conexion(&KERNEL->CXs[puntero_pkt->cabecera.id_destino].barC);
                        break;
                        
                    case CR:
                        fprintf(stderr,"\nRecibido un CONEXION REQUEST");
                        fprintf(stderr,"\npuntero_pkt->cabecera.puerto_orig: %d",puntero_pkt->cabecera.puerto_orig);
                        fprintf(stderr,"\npuntero_pkt->cabecera.puerto_dest: %d",puntero_pkt->cabecera.puerto_dest);
                        fprintf(stderr,"\npuntero_pkt->cabecera.id_destino: %d",puntero_pkt->cabecera.id_destino);
                        fprintf(stderr,"\npuntero_pkt->cabecera.id_local: %d",puntero_pkt->cabecera.id_local);
                        fprintf(stderr,"\nip_local: %s",inet_ntop(AF_INET,&(KERNEL->CXs[0].ip_local.s_addr),ipcharbuf,20));
                       
                        //comprobar si tiene conexcion preparada en listen
                        resul = asign_conexion_CR(ip_remota,puntero_pkt);
                        it_libres = buscar_buffer_libre();
                        fprintf(stderr,"\nLe asigno al connect la conexion: %d",resul);
                        if (resul == EXNOTSAP) {
                            //paquete.cabecera.conexion_aceptada = resul;
                            it_libres->pkt->cabecera.conexion_aceptada=resul;
                            KERNEL->CXs[resul].resultado_peticion = resul;
                        }else {
                            //paquete.cabecera.conexion_aceptada= 1;
                            it_libres->pkt->cabecera.conexion_aceptada=1;
                            KERNEL->CXs[resul].estado_cx = ESTABLISHED;
                            KERNEL->CXs[resul].resultado_peticion = EXOK;
                            KERNEL->CXs[resul].puerto_destino = puntero_pkt->cabecera.puerto_orig;
                            KERNEL->CXs[resul].ip_destino = ip_remota;
                            KERNEL->CXs[resul].id_destino = puntero_pkt->cabecera.id_local;
                           //iniciamos el NUMERO DE SECUENCIA para esta conexion
                            KERNEL->CXs[resul].numero_secuencia = 0;
                        }
                        fprintf(stderr,"\nid_destino es: %d",puntero_pkt->cabecera.id_local);
                        fprintf(stderr,"\nid_local es: %d",puntero_pkt->cabecera.id_destino);
                        //desbloquear_acceso(&KERNEL->SEMAFORO);
                        //completamos tsap_origen y tsap_destino
                        tsap_origen.ip.s_addr = KERNEL->CXs[resul].ip_local.s_addr;
                        tsap_origen.puerto = puntero_pkt->cabecera.puerto_dest;
                        tsap_destino.ip.s_addr = ip_remota.s_addr;
                        tsap_destino.puerto = puntero_pkt->cabecera.puerto_orig;
                        
                        //it_libres = buscar_buffer_libre();
                        //rellenamos los datos
                        it_libres->contador_rtx = NUM_MAX_RTx;
                        
                        //it_tx = KERNEL->CXs[resul].TX.end();
                        //KERNEL->CXs[resul].TX.splice(it_tx,KERNEL->buffers_libres,it_libres);
                        //it_tx = --KERNEL->CXs[resul].TX.end();
                        
                        //creamos paquete CC
                        fprintf(stderr,"\nCreamos pakete");
                         crear_pkt(it_libres->pkt,CC,&tsap_destino,&tsap_origen,NULL,0,resul,puntero_pkt->cabecera.id_local);
                         fprintf(stderr,"\nEnviamos pakete");
                         enviar_tpdu(ip_remota,it_libres->pkt,sizeof(tpdu));
                         fprintf(stderr,"\nEnviamos tpdu");
                         //despertamos al listen
                         fprintf(stderr,"\nvamos a ejecutar despierta_conexion()");
                         fprintf(stderr,"\ndespertamos la conexion de indice: %d",resul);
                         fprintf(stderr,"%d",KERNEL->CXs[resul].puerto_origen);
                         fprintf(stderr,"%d",KERNEL->CXs[resul].puerto_destino);
                         desbloquear_acceso(&KERNEL->SEMAFORO);
                         despierta_conexion(&KERNEL->CXs[resul].barC);
                         //desbloquear_acceso(&KERNEL->SEMAFORO);
                         //despierta_conexion(&KERNEL->barrera);
                         fprintf(stderr,"\nDespertamos conexion\n");
                        break;
                    case ACK:
                        fprintf(stderr,"\nRecibido un ACK");
                        indice = KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX.size();// lo hago porque size() varia en el bucle
                        it_tx = KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX.begin();//apunta al principio de TX
                        //LIBERAMOS TODOS LOS PAKETES ASENTIDOS
                        for(i=0;(uint)i < (uint)indice;i++){
                            //miramos a que buffer de TX asiente y pasamos a buffer libres
                            if(puntero_pkt->cabecera.numero_secuencia >= it_tx->num_secuencia){
                                it_tx->estado_pkt = confirmado;
                                it_libres = KERNEL->buffers_libres.begin();
                                KERNEL->buffers_libres.splice(it_libres,KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX,it_tx);
                                it_tx++;//avanzamos el iterador al siguiente buffer de TX
                                
                                //miramos si hay que mandar un DR
                                if((KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX.empty())
                                        &&(KERNEL->CXs[puntero_pkt->cabecera.id_destino].signal_disconnect == true)) {
                                    //rellenamos datos de los TSAPs
                                    //t_direccion tsap_origen, tsap_destino;
                                    tsap_origen.ip.s_addr = KERNEL->CXs[puntero_pkt->cabecera.id_destino].ip_local.s_addr;
                                    tsap_origen.puerto = puntero_pkt->cabecera.puerto_dest;
                                    tsap_destino.ip.s_addr = ip_remota.s_addr;
                                    tsap_destino.puerto = puntero_pkt->cabecera.puerto_orig;
                                    
                                    //mandamos el DR a través del buffer TX
                                    it_libres = buscar_buffer_libre();
                                    it_tx = KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX.end();
                                    KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX.splice(it_tx, KERNEL->buffers_libres, it_libres);
                                    it_tx = --KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX.end();
                                    crear_pkt(it_tx->pkt, DR, &tsap_destino, &tsap_origen, NULL, 0, puntero_pkt->cabecera.id_destino, puntero_pkt->cabecera.id_local);
                                    enviar_tpdu(tsap_destino.ip, it_tx->pkt, sizeof (tpdu));
                                    
                                    //ahora hay que avisar al send si esta dormido, que de un EXCLOSE
                                    if(KERNEL->CXs[puntero_pkt->cabecera.id_destino].primitiva_dormida == true){
                                        KERNEL->CXs[puntero_pkt->cabecera.id_destino].primitiva_dormida = false;
                                        desbloquear_acceso(&KERNEL->SEMAFORO);
                                        despierta_conexion(&KERNEL->CXs[puntero_pkt->cabecera.id_destino].barC);
                                    }
                                }
                                
                                //DESPUES DE MANDAR EL DR no despuerto  SEND porque habrá finalizado con EXDISC
                                
                                //si hay HUECO en buffer TX llamamos a la primitiva si no hay un DISCONNECT
                                if((KERNEL->CXs[puntero_pkt->cabecera.id_destino].primitiva_dormida == true)
                                        &&(KERNEL->CXs[puntero_pkt->cabecera.id_destino].signal_disconnect==false)){
                                    KERNEL->CXs[puntero_pkt->cabecera.id_destino].primitiva_dormida = false;
                                    desbloquear_acceso(&KERNEL->SEMAFORO);
                                    despierta_conexion(&KERNEL->CXs[puntero_pkt->cabecera.id_destino].barC);
                                }
                            }        
                       }
                        break;
                        
                    case DATOS:
                        fprintf(stderr,"\nRecibido un DATOS");
                        //miramos si hay sitio en buffer RX
                        if((int)KERNEL->CXs[puntero_pkt->cabecera.id_destino].RX.size() < NUM_BUF_PKTS){
                            fprintf(stderr,"\nBPRINCIPAL: hay sitio en el buffer de RX");
                            //miramos si es el num_seq esperado
                            if(puntero_pkt->cabecera.numero_secuencia == KERNEL->CXs[puntero_pkt->cabecera.id_destino].numero_secuencia){
                                fprintf(stderr,"\nBPRINCIPAL: el numero de secuencia es el que estoy esperando");
                                //it_libres = buscar_buffer_libre();
                                it_libres->bytes_restan = puntero_pkt->cabecera.tamanho_datos;
                                it_libres->ultimo_byte = it_libres->pkt->datos;
                                //memcpy(it_libres->contenedor,puntero_pkt,sizeof(tpdu));
                                fprintf(stderr,"\nBPRINCIPAL: copiado el contenido del pkt al buffer");
                                it_rx = KERNEL->CXs[puntero_pkt->cabecera.id_destino].RX.end();
                                KERNEL->CXs[puntero_pkt->cabecera.id_destino].RX.splice(it_rx,KERNEL->buffers_libres,it_libres);
                                fprintf(stderr,"\nBPRINCIPAL: pasamos el buffer de libres a recepcion");
                                //rellenamos datos de los TSAPs
                                //t_direccion tsap_origen, tsap_destino;
                                tsap_origen.ip.s_addr = KERNEL->CXs[puntero_pkt->cabecera.id_destino].ip_local.s_addr;
                                tsap_origen.puerto = puntero_pkt->cabecera.puerto_dest;
                                tsap_destino.ip.s_addr = ip_remota.s_addr;
                                tsap_destino.puerto = puntero_pkt->cabecera.puerto_orig;
                                
                                //miramos si la conexion remota está cerrando su flujo
                                //si se cumple, avisamos al KERNEL del evento
                                if(puntero_pkt->cabecera.close == 1){
                                    fprintf(stderr,"\nBPRINCIPAL: puntero_pkt->cabecera.close == 1");
                                    KERNEL->CXs[puntero_pkt->cabecera.id_destino].desconexion_remota = true;
                                }
                                
                                //construimos el ACK y enviamos
                                it_libres = buscar_buffer_libre();
                                fprintf(stderr,"\nBPRINCIPAL: construimos ACK");
                                crear_pkt(it_libres->pkt,ACK,&tsap_destino,&tsap_origen,NULL,0,puntero_pkt->cabecera.id_destino,puntero_pkt->cabecera.id_local);
                                KERNEL->CXs[puntero_pkt->cabecera.id_destino].numero_secuencia++;//incrementamos numero_secuencia
                                fprintf(stderr,"\nBPRINCIPAL: enviamos tpdu ACK");
                                enviar_tpdu(ip_remota,it_libres->pkt,sizeof(tpdu));
                                
                                //miramos si despertamos al receive
                                if(KERNEL->CXs[puntero_pkt->cabecera.id_destino].primitiva_dormida == true){
                                    KERNEL->CXs[puntero_pkt->cabecera.id_destino].primitiva_dormida = false;
                                    fprintf(stderr,"\nBRPINCIPAL: despertamos al receive");
                                    desbloquear_acceso(&KERNEL->SEMAFORO);
                                    despierta_conexion(&KERNEL->CXs[puntero_pkt->cabecera.id_destino].barC);
                                }
                            }
                        }
                        break;
                        
                    case DR:
                        fprintf(stderr,"\nRecibido un DISCONECTION REQUEST");
                        //avisamos en el KERNEL de que la entidad remota solicito un DISCONNECT
                        KERNEL->CXs[puntero_pkt->cabecera.id_destino].signal_disconnect = true;
                        //rellenamos los tsaps
                        tsap_origen.ip.s_addr = KERNEL->CXs[puntero_pkt->cabecera.id_destino].ip_local.s_addr;
                        tsap_origen.puerto = puntero_pkt->cabecera.puerto_dest;
                        tsap_destino.ip.s_addr = ip_remota.s_addr;
                        tsap_destino.puerto = puntero_pkt->cabecera.puerto_orig;
                        //construimos el paquete DC y enviamos
                        crear_pkt(&paquete,DC,&tsap_destino,&tsap_origen,NULL,0,puntero_pkt->cabecera.id_destino,puntero_pkt->cabecera.id_local);
                        enviar_tpdu(ip_remota,&paquete,sizeof(tpdu));
                        //liberamos los recursos
                        KERNEL->buffers_libres.splice(KERNEL->buffers_libres.begin(), KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX);
                        KERNEL->buffers_libres.splice(KERNEL->buffers_libres.begin(), KERNEL->CXs[puntero_pkt->cabecera.id_destino].RX);
                        KERNEL->CXs[puntero_pkt->cabecera.id_destino].celda_ocupada = 0;
                        KERNEL->CXs[puntero_pkt->cabecera.id_destino].estado_cx = CLOSED;
                        //miramos si hay que despertar a la primitiva
                        if(KERNEL->CXs[puntero_pkt->cabecera.id_destino].primitiva_dormida == true){
                            KERNEL->CXs[puntero_pkt->cabecera.id_destino].primitiva_dormida = false;
                            desbloquear_acceso(&KERNEL->SEMAFORO);
                            despierta_conexion(&KERNEL->CXs[puntero_pkt->cabecera.id_destino].barC);
                            
                        }
                        break;
                    case DC:
                        fprintf(stderr,"\nRecibido un DISCONECTION CONFIRM");
                        //???QUE HACER?
                        //liberamos los recursos
                        KERNEL->buffers_libres.splice(KERNEL->buffers_libres.begin(),KERNEL->CXs[puntero_pkt->cabecera.id_destino].TX);
                        KERNEL->buffers_libres.splice(KERNEL->buffers_libres.begin(), KERNEL->CXs[puntero_pkt->cabecera.id_destino].RX);
                        KERNEL->CXs[puntero_pkt->cabecera.id_destino].celda_ocupada = 0;
                        KERNEL->CXs[puntero_pkt->cabecera.id_destino].estado_cx = CLOSED;
                        //liberamos la conexion
                        break;

                }
                
                break;
        }

    } while (daemon_stop != true);
}

extern char dir_proto[]; // "s.ltmdg" por defecto
extern char interfaz[]; // "eth0" por defecto
extern int LTM_PROTOCOL;
extern int pe;
extern int NUM_BUF_PKTS; // 100 por defecto

int procesa_argumentos(int argc, char ** argv) {

    //////////////////////////////////////////////////////////////
    // ltmd [-s dir_proto]   [-e perror]   [-p ltm_protocol]    //
    //    [-i interfaz] [-n NUM_BUF_PKTS]                       //
    //                                                          //
    //* perror debe ser un entero entre 0 y 100.                //
    //                                                          //
    //                                                          //
    //                                                          //
    //  char dir_proto[64]                                      //
    //  int pe                                                  //
    //  int LTM_PROTOCOL                                        //    
    //  char interfaz[8]                                        //
    //  int NUM_BUF_PKTS                                        //
    //////////////////////////////////////////////////////////////

    LTM_PROTOCOL = 233;
    int opt;
    
        while ((opt = getopt(argc, argv, "s:e:p:i:n:")) != -1) {
            switch(opt){
                case 's':
                    strcpy(dir_proto, optarg);
                    fprintf(stderr,"\ndir_proto: %s\n",dir_proto);
                    break;
                case 'e':
                    pe = atoi(optarg);
                    fprintf(stderr,"\npe: %d\n",pe);
                    break;
                case 'p':
                    LTM_PROTOCOL = atoi(optarg);
                    fprintf(stderr,"\nLTM_PROTOCOL: %d\n",LTM_PROTOCOL);
                    break;
                case 'i':
                    strcpy(interfaz,optarg);
                    fprintf(stderr,"\ninterfaz: %s\n",interfaz);
                    break;
                case 'n':
                    NUM_BUF_PKTS = atoi(optarg);
                    fprintf(stderr,"\nNUM_BUF_PKTS: %d\n",NUM_BUF_PKTS);
                    break;
            }
                    
        }
    
    return EXOK;
}


