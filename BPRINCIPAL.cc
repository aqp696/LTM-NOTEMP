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
    int i;
    //inicializamos cada conexion
    for(i=0;i<NUM_MAX_CXs;i++){
        memset(&KERNEL->CXs[i],0,sizeof(conexion_t));
        KERNEL->CXs[i].estado_cx = CLOSED;
        INICIA_LISTA(KERNEL->CXs[i].TX,buf_pkt);
        INICIA_LISTA(KERNEL->CXs[i].RX,buf_pkt);
        INICIA_LISTA(KERNEL->CXs[i].buffers_libres,buf_pkt);
    }
    desbloquear_acceso(&KERNEL->SEMAFORO);

    //Inicializamos la lista de conexiones libres
    INICIA_LISTA(KERNEL->CXs_libres,int);
    inicializar_CXs_libres();

    do {

        switch (ltm_wait4event(shortest)) {

            case TIME_OUT:
                fprintf(stderr, "El protocolo despierta por TMOUT (hora %ld)\n", time(0));

                break;

            case INTERRUP:
                break;

            case PAQUETE:                
                fprintf(stderr,"\n (%d) Se ha recibido un PAQUETE ", cont + 1);

                int rec = recibir_tpdu(pkt, MAX_LONG_PKT, &ip_remota, &offset);
                if (rec >= 0) {
                    fprintf(stderr,"desde la IP %s\ntexto del mensaje: %s\n",
                            inet_ntop(AF_INET, &ip_remota, ipcharbuf, 20), pkt + offset);
                }
                puntero_pkt = (tpdu *)(pkt+offset);
                
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
                        }
                        desbloquear_acceso(&KERNEL->SEMAFORO);
                        fprintf(stderr,"\nid_estino es: %d",puntero_pkt->cabecera.id_local);
                        fprintf(stderr,"\nid_local es: %d",puntero_pkt->cabecera.id_destino);
                        despierta_conexion(&KERNEL->CXs[puntero_pkt->cabecera.id_destino].barC);
                        break;
                        
                    case CR:
                        fprintf(stderr,"\nRecibido un CONEXION REQUEST");
                        //comprobar si tiene conexcion preparada en listen
                        int resul = asign_conexion_CR(puntero_pkt,KERNEL);
                        fprintf(stderr,"\nLe asigno al connect la conexion: %d",resul);
                        if (resul == EXNOTSAP) {
                            paquete.cabecera.conexion_aceptada = resul;
                            KERNEL->CXs[resul].resultado_peticion = resul;
                        }else {
                            paquete.cabecera.conexion_aceptada= 1;
                            KERNEL->CXs[resul].estado_cx = ESTABLISHED;
                            KERNEL->CXs[resul].resultado_peticion = EXOK;
                            KERNEL->CXs[resul].puerto_destino = puntero_pkt->cabecera.puerto_orig;
                            KERNEL->CXs[resul].ip_destino = ip_remota;
                            KERNEL->CXs[resul].id_destino = puntero_pkt->cabecera.id_local;
                        }
                        fprintf(stderr,"\nid_destino es: %d",puntero_pkt->cabecera.id_local);
                        fprintf(stderr,"\nid_local es: %d",puntero_pkt->cabecera.id_destino);
                        desbloquear_acceso(&KERNEL->SEMAFORO);
                        //completamos tsap_origen y tsap_destino
                        tsap_origen.ip = KERNEL->CXs[resul].ip_local;
                        tsap_origen.puerto = puntero_pkt->cabecera.puerto_dest;
                        tsap_destino.ip = ip_remota;
                        tsap_destino.puerto = puntero_pkt->cabecera.puerto_orig;
                        //creamos paquete CC
                        fprintf(stderr,"\nCreamos pakete");
                         crear_pkt(&paquete,CC,&tsap_destino,&tsap_origen,NULL,0,resul,puntero_pkt->cabecera.id_local);
                         fprintf(stderr,"\nEnviamos pakete");
                         enviar_tpdu(ip_remota,&paquete,sizeof(paquete));
                         fprintf(stderr,"\nEnviamos tpdu");
                         //despertamos al listen
                         despierta_conexion(&KERNEL->CXs[resul].barC);
                         fprintf(stderr,"\nDespertamos conexion\n");
                        break;
                }

//                if (++cont == 1)
//                    despierta_conexion(&KERNEL->CXs[0].barC);
//
//                sleep(3);
//
//                snprintf(pkt, sizeof(pkt), "saludos desde protocolo %d\n", KERNEL->kernel_pid);
//                enviar_tpdu(ip_remota, pkt, 700);
                
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
                    strcpy(dir_proto,optarg);
                    fprintf(stderr,"\ndir_proto: %s\n",dir_proto);
                    break;
                case 'e':
                    pe =   atoi(optarg);
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


