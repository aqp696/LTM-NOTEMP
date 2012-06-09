#include "CONFP.h"
#include "ltmdaemon.h"
#include "ltmipcs.h"
//esto es mio
#include "string.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>


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
    fprintf(stderr,"\nbuscar_buffer_libre: tamanho %d",KERNEL->buffers_libres.size());
    if(KERNEL->buffers_libres.empty()) {
        fprintf(stderr,"\nla lista de libres esta vacia-> creación de un buf_pkt");
        buf_pkt nodo;
        KERNEL->buffers_libres.push_back(nodo);
    }else{
        fprintf(stderr,"\nla lista de libres no esta vacia");
    }
    it_libres = --KERNEL->buffers_libres.end();
    it_libres->pkt = (tpdu *)it_libres->contenedor;
    fprintf(stderr,"\nDentro de la funcion buscar_buffer_libre()");
    return it_libres;
}

list<evento_t, shm_Allocator<evento_t> >::iterator buscar_temporizador_libre(){
    list<evento_t, shm_Allocator<evento_t> >::iterator it_tempo_libres;
    evento_t nodo;
    nodo.timeout = 0;
    nodo.indice_cx = -1;
    nodo.tipo_tempo = -1;
    if(KERNEL->temporizadores_libres.empty()){
        KERNEL->temporizadores_libres.push_back(nodo);
    }
    it_tempo_libres = KERNEL->temporizadores_libres.begin();

    return it_tempo_libres;
}

uint32_t tiempo_actual(){
    uint32_t tiempo;
    timeval tv;
    gettimeofday(&tv,NULL);

    tiempo = tv.tv_sec*1000+tv.tv_usec/1000;
    return tiempo;
}

void recalcular_temporizador_red(int id) {
    list<evento_t, shm_Allocator<evento_t> >::iterator it_temp;
    it_temp = KERNEL->tout_red_aplic.end();
    KERNEL->CXs[id].it_tempo_red->timeout = tiempo_rtx_red;
    //pasamos el temporizador al final de la lista de temporizadores de red_aplic
    KERNEL->tout_red_aplic.splice(it_temp,KERNEL->tout_red_aplic,KERNEL->CXs[id].it_tempo_red);
}

void recalcular_temporizador_aplic(int id){
    list<evento_t, shm_Allocator<evento_t> >::iterator it_temp;
    it_temp = KERNEL->tout_red_aplic.end();
    KERNEL->CXs[id].it_tempo_aplic->timeout=tiempo_rtx_aplic;
    //pasamos el temporizador al final de la lista de temporizadores de red_aplic
    KERNEL->tout_red_aplic.splice(it_temp,KERNEL->tout_red_aplic,KERNEL->CXs[id].it_tempo_aplic);
}

int calcular_shortest() {
    int valor_shortest = 5000;
    uint32_t tiempo_pkt = 0, tiempo_red_aplic = 0, hora_actual;
    list<evento_t, shm_Allocator<evento_t> >::iterator it_tempo_pkt;
    list<evento_t, shm_Allocator<evento_t> >::iterator it_tempo_red_aplic;

    hora_actual = tiempo_actual();

    // si las listas de temporizadores estan vacias entonces -> VALOR POR DEFECTO = 5seg
    if ((KERNEL->tout_pkts.empty()) && (KERNEL->tout_red_aplic.empty())) {
        //KERNEL->tipo_timeout = timeout_normal;
        return valor_shortest;
    }

    //si hay temporizador de red_aplic miramos su valor de timeout
    if (!KERNEL->tout_red_aplic.empty()) {
        it_tempo_red_aplic = KERNEL->tout_red_aplic.begin();
        tiempo_red_aplic = it_tempo_red_aplic->timeout;
    }

    //si hay temporizador de pkt miramos su valor de timeout
    if (!KERNEL->tout_pkts.empty()) {
        it_tempo_pkt = KERNEL->tout_pkts.begin();
        tiempo_pkt = it_tempo_pkt->timeout;
    }

    //comparamos los valores de los temporizadores para saber cual vencio antes
    if (tiempo_pkt < tiempo_red_aplic) {
        if (tiempo_pkt - hora_actual >= 0) {
            valor_shortest = tiempo_pkt - hora_actual;
            //KERNEL->it_tipo_vencimiento = it_tempo_pkt;
            //KERNEL->tipo_timeout = timeout_pkt;
        }else{
            valor_shortest = 0;
            //KERNEL->tipo_timeout = timeout_normal;
        }
    } else {
        if (tiempo_red_aplic - hora_actual >= 0) {
            valor_shortest = tiempo_red_aplic - hora_actual;
            //KERNEL->it_tipo_vencimiento = it_tempo_red_aplic;
            //KERNEL->tipo_timeout = timeout_red_aplic;
        }else{
            valor_shortest = 0;
            //KERNEL->tipo_timeout = timeout_normal;
        }
    }

    return valor_shortest;
}

void comprobar_vencimientos(){
    uint32_t hora_actual = tiempo_actual();
    uint32_t tiempo_pkt = 0, tiempo_red_aplic = 0;
    list<evento_t, shm_Allocator<evento_t> >::iterator it_tempo_pkt;
    list<evento_t, shm_Allocator<evento_t> >::iterator it_tempo_red_aplic;

    //miramos si fue un timeout normal
    if((KERNEL->tout_pkts.empty())&&(KERNEL->tout_red_aplic.empty())){
        KERNEL->tipo_timeout = timeout_normal;
    }

    //miramos el timeout del temporizador_pkt
    if(!KERNEL->tout_pkts.empty()){
        it_tempo_pkt = KERNEL->tout_pkts.begin();
        tiempo_pkt = it_tempo_pkt->timeout;
    }

    //miramos el timeout del temporizador red_aplic
    if(!KERNEL->tout_red_aplic.empty()){
        it_tempo_red_aplic = KERNEL->tout_red_aplic.begin();
        tiempo_red_aplic = it_tempo_red_aplic->timeout;
    }

    //miramos qué temporizador venció antes
    if(tiempo_pkt <= tiempo_red_aplic){
        if(tiempo_pkt - hora_actual <= 0){
            KERNEL->it_tipo_vencimiento = it_tempo_pkt;
            KERNEL->tipo_timeout = timeout_pkt;
        }else{
            KERNEL->tipo_timeout = timeout_normal;
        }
    }else{//tiempo_red_aplic < tiempo_pkt
        if(tiempo_red_aplic - hora_actual <= 0){
            KERNEL->it_tipo_vencimiento = it_tempo_red_aplic;
            KERNEL->tipo_timeout = timeout_red_aplic;
        }
    }

}