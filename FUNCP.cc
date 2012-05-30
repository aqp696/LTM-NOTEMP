#include "CONFP.h"
#include "ltmdaemon.h"
#include "ltmipcs.h"
//esto es mio
#include "string.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


void ejemplo(int argumento) {

    // aquí la implementacion de la función
}

uint16_t buscar_puerto_libre() {
    uint16_t puerto;
    int puerto_ocupado = 0;
    int i;
    do {
        puerto_ocupado = 0;
        srand(time(NULL));
        puerto = (uint16_t) (1 + rand() % 32767);
        for (i = 0; i < NUM_MAX_CXs; i++) {
            if (KERNEL->CXs[i].puerto_origen == puerto) {
                puerto_ocupado = 1;
                break;
            }
        }
    } while (puerto_ocupado > 0);
    return puerto;
}

int comprobar_parametros(const t_direccion *tsap_destino, t_direccion *tsap_origen, char tipo) {


    switch (tipo) {
        case 'c':
            //comprobamos IP tsap origen, si es cero obtenemos ip_local
            if ((tsap_origen->ip).s_addr == 0) {
                printf("\nObtenemos ip local");
                if (obtener_IP_local((KERNEL->i_red).interfaz, &tsap_origen->ip) == 0) {
                    printf("\nError al obtener_IP_local");
                    return -1;
                }
            }
            //comprobamos PUERTO tsap origen, si es cero le asignamos uno
            if (tsap_origen->puerto == 0) {
                int16_t puerto_aux;
                puerto_aux = buscar_puerto_libre();
                fprintf(stderr,"\nel puerto  es %d",puerto_aux);
                tsap_origen->puerto = puerto_aux;
            }
            
            //si la ip_destino o el puerto destino es 0 damos error
            if ((tsap_destino->puerto == 0) || ((tsap_destino->ip).s_addr == 0)) {
                return -1;
            }

            break;
        case 'l':
            //si la ip de tsap_escucha es nula  lo completamos con la ip_local
            if ((tsap_origen->ip).s_addr == 0) {
                printf("\nObtenemos ip local");
                if (obtener_IP_local((KERNEL->i_red).interfaz, &(tsap_origen->ip)) == 0) {
                    printf("\nError al obtener_IP_local");
                    return -1;
                }
                printf("\niplocal es: %s",inet_ntoa(tsap_origen->ip));
            }
            // si el puerto origen es cero damos error
            if(tsap_origen->puerto == 0){
                printf("\nError: el puerto es cero");
                return -1;
            }
            

            break;
            
        case 's':
            break;
        case 'r':
            break;
    }

    return 0;
}


int buscar_celda_libre() {
    int i;
    int indice=0;
    for (i = 0; i < NUM_MAX_CXs; i++) {
        if (KERNEL->CXs[i].celda_ocupada == 0) {
            indice = i;
            break;
        }
    }
    //indice = KERNEL->CXs_libres.front();
    return indice;
}

void crear_pkt(tpdu *pkt,char tipo, t_direccion *tsap_destino,t_direccion *tsap_origen,void *datos,int longitud,int id_local,int id_destino){
    printf("\nEstamos creando el paquete");
    pkt->cabecera.tipo = tipo;
    pkt->cabecera.puerto_dest = tsap_destino->puerto;
    pkt->cabecera.puerto_orig = tsap_origen->puerto;
    pkt->cabecera.ip_destino = tsap_destino->ip;
    //pkt->cabecera.ip_local = tsap_origen->ip;
    pkt->cabecera.close = 0;
    pkt->cabecera.id_destino = id_destino;
    pkt->cabecera.id_local = id_local;
    pkt->cabecera.tamanho_datos=longitud;
    pkt->cabecera.numero_secuencia = KERNEL->CXs[id_local].numero_secuencia;
    fprintf(stderr,"\nCopiamos los datos al pakete");
   
        memcpy (pkt->datos,datos,longitud);
 
}

int asign_conexion_CR(struct in_addr ip_remota,tpdu *puntero_pkt) {
    int i;
    int indice = EXNOTSAP;
    for (i = 0; i < NUM_MAX_CXs; i++) {
        //fprintf(stderr,"\nip_remota.s_addr: %s; KERNEL->CXs[%d].ip_local.s_addr: %s",inet_ntop(AF_INET, &ip_remota, ipcharbuf, 20),i,KERNEL->);
        
        if((puntero_pkt->cabecera.ip_destino.s_addr == KERNEL->CXs[i].ip_local.s_addr)
                &&(puntero_pkt->cabecera.puerto_dest == KERNEL->CXs[i].puerto_origen)){
        if(
                //ip_destino cero, puerto cero y listen
                ((KERNEL->CXs[i].ip_destino.s_addr==0)&&(KERNEL->CXs[i].puerto_destino==0)&&(KERNEL->CXs[i].estado_cx==LISTEN))||
                //ip_destino==tsap_destino->ip y puerto cero y listen
                ((KERNEL->CXs[i].ip_destino.s_addr==ip_remota.s_addr)&&(KERNEL->CXs[i].puerto_destino==0)&&(KERNEL->CXs[i].estado_cx==LISTEN))||
                //ip_destino==tsap_destino->ip y puerto-destino == tsap_destino->puerto y listen
                ((KERNEL->CXs[i].ip_destino.s_addr==ip_remota.s_addr)&&(KERNEL->CXs[i].puerto_destino==puntero_pkt->cabecera.puerto_orig)&&(KERNEL->CXs[i].estado_cx==LISTEN))
                ){
            indice=i;
            break;
        }
        }
    }
//        if ((((KERNEL->CXs[i].ip_destino.s_addr == 0 ||
//                (KERNEL->CXs[i].ip_destino.s_addr == puntero_pkt->cabecera.ip_local.s_addr)) &&
//                ((KERNEL->CXs[i].puerto_destino == 0) || (KERNEL->CXs[i].puerto_destino == puntero_pkt->cabecera.puerto_orig)) &&
//                (KERNEL->CXs[i].estado_cx == LISTEN)) {
//            indice = i;
//            break;
//        }
//    }
    return indice;
}

void inicializar_CXs_libres(){
    int i;
    for(i=0;i<NUM_MAX_CXs;i++){
        KERNEL->CXs_libres.push_back(i);
    }
}

int buscar_connect_repetido(t_direccion *tsap_origen, const t_direccion *tsap_destino){
    int i;
    int encontrada = -1;
    for(i=0;i<NUM_MAX_CXs;i++){
        if(((KERNEL->CXs[i].ip_destino.s_addr==tsap_destino->ip.s_addr)&&
                (KERNEL->CXs[i].puerto_destino==tsap_destino->puerto))||
                (KERNEL->CXs[i].puerto_origen==tsap_origen->puerto)){
            encontrada=i;
            fprintf(stderr,"\nconnect repetido");
            break;
        }
    }
    //si esta repetida se devuelve algo distinto de cero, si no se encuentra encontrada = 0
    return encontrada;
}

int buscar_listen_repetido(t_direccion *tsap_escucha, t_direccion *tsap_remota){
    int i;
    int encontrada = -1;
    for(i=0;i<NUM_MAX_CXs;i++){
        if((KERNEL->CXs[i].puerto_origen==tsap_escucha->puerto)){
            encontrada = i;
            break;
        }
    }
    
    return encontrada;
}

list<buf_pkt, shm_Allocator<buf_pkt> >::iterator buscar_buffer_libre(){
    list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_libres;
    buf_pkt nodo;
    nodo.pkt = (tpdu *) nodo.contenedor;
    if(KERNEL->buffers_libres.empty()){
        KERNEL->buffers_libres.push_back(nodo);
    }
    it_libres = KERNEL->buffers_libres.begin();
    fprintf(stderr,"\nDentro de la funcion buscar_buffer_libre()");
    return it_libres;
}