#ifndef _CONFP_H
#define _CONFP_H

#include "ltmtypes.h"
#include "comunicacion.h"
#include "ltmipcs.h"

#include <list>
#include <stdbool.h>
#include "ltmallocator.h"
using namespace std;

//TIPOS DE PAQUETES
#define CR 101
#define CC 102
#define DATOS 103
#define ACK 104
#define DR 105
#define DC 106

//ESTADOS DE LA CONEXION
#define CLOSED 201
#define LISTEN 202
#define CONNECT 203
#define ESTABLISHED 204
#define FIN_WAIT1 205
#define FIN_WAIT2 206

#define DEPURA 0

//CALCULO DE VENCIMIENTO TEMPORIZADORES
#define tiempo_rtx_pkt tiempo_actual()-KERNEL->t_inicio+200
#define tiempo_rtx_red tiempo_actual() -KERNEL->t_inicio+60000
#define tiempo_rtx_aplic tiempo_actual() -KERNEL->t_inicio+60000
#define NUM_MAX_RTx 6

//ESTADOS DE ENVIO DE PKT
#define no_confirmado 1
#define confirmado 2

//TIPOS DE VENCIMIENTO DE PKT
#define vencimiento_pkt 300
#define vencimiento_red 301
#define vencimiento_aplic 302

//TIPOS DE TIMEOUT
#define timeout_normal 1
#define timeout_pkt 2
#define timeout_red_aplic 3

// INICIACION DE UNA LISTA EN LA ZONA DE MEMORIA COMPARTIDA 

#define INICIA_LISTA(lista,tipo_nodo)				\
  reinterpret_cast<list<tipo_nodo,shm_Allocator<tipo_nodo> > *>	\
        (new(&lista) list<tipo_nodo,shm_Allocator<tipo_nodo> >);


#define CABECERA_TPDU sizeof(cab_tcp)
// una vez definida la estructura de la cabecera 
// de la TPDU que lleva los datos ese valor 20 debe 
// reemplazarse por sizeof(tipo cabecera)

#define MAX_LONG_PKT CABECERA_IP+CABECERA_TPDU+MAX_DATOS

typedef struct _cab_tcp {
    uint8_t tipo; //CC,CR,DR,DATOS,ACK
    uint8_t id_local;
    uint8_t id_destino;
    uint16_t puerto_orig;
    uint16_t puerto_dest;
    //struct in_addr ip_local;
    struct in_addr ip_destino;
    uint8_t conexion_aceptada; // se usa como CCpos y CCneg, o como ultimo pakete si flags = BLOCK
    uint16_t tamanho_datos;
    uint32_t numero_secuencia;
    uint8_t close;
    //..mas datos
}cab_tcp;

typedef struct _tpdu{
    cab_tcp cabecera;
    char datos[MAX_DATOS];
}tpdu;

typedef struct _buf_pkt buf_pkt;

typedef struct _evento_t{
    list<buf_pkt, shm_Allocator<buf_pkt> >::iterator it_pkt;//iterador a buffer
    uint32_t timeout;
    int indice_cx;
    int tipo_tempo;
}evento_t;

typedef struct _buf_pkt {
    // nbytes
    // id. conexion
    // ...
    char *ultimo_byte;//ultimo byte copiado del buffer
    unsigned int bytes_restan;
    int estado_pkt;//asentido o no
    int contador_rtx;//para saber si se agotaron las rtx
    uint num_secuencia;
    list<evento_t,shm_Allocator<evento_t> >::iterator it_tout_pkt;//iterador a evento_t
    tpdu *pkt;
    
    char contenedor[MAX_LONG_PKT];
} buf_pkt;

typedef struct _conexion {
  pid_t ap_pid;
  barrera_t barC;
  //esto es mio
  uint16_t puerto_origen;
  uint16_t puerto_destino;
  int celda_ocupada;//nos dice si se puede asignar esta conexion
  int estado_cx;
  int resultado_peticion;
  int id_destino;
  int ultimo_ack;
  uint numero_secuencia;
  bool signal_disconnect;// señal del disconnect, para avisar de que terminemos el flujo
  bool desconexion_remota;
  struct in_addr ip_destino;
  struct in_addr ip_local;
  
  bool primitiva_dormida;
  int resultado_primitiva;//aki es donde ponemos si fue un EXNET,OK ...
  // semaforo?
  // lista de paquetes/buf. pendientes de asentimiento
  // lista de paquetes/buf. recibidos
  // ...

  list<evento_t, shm_Allocator<evento_t> >::iterator it_tempo_red;//iterador al temporizador de red
  list<evento_t, shm_Allocator<evento_t> >::iterator it_tempo_aplic;//iterador al temporizador de aplicacion
  
  list<buf_pkt, shm_Allocator<buf_pkt> >TX; //bufferes de transmision
  list<buf_pkt, shm_Allocator<buf_pkt> >RX; //bufferes de recepcion
  
} conexion_t;

typedef struct _kernel_shm {
  interfaz_red_t i_red; // este campo debe ser el primero de esta estructura
  pid_t kernel_pid;
  barrera_t barrera;
  //esto es mio
  int num_CXs;
  int indice_libre;
  semaforo_t SEMAFORO;
  uint16_t NUM_BUF_PKTS;
  uint32_t t_inicio;
  uint8_t tipo_timeout;
  list<evento_t, shm_Allocator<evento_t> >::iterator it_temporizador_vencido;
  list<buf_pkt, shm_Allocator<buf_pkt> >buffers_libres;
  list<int, shm_Allocator<int> >CXs_libres;

  list<evento_t, shm_Allocator<evento_t> >tout_pkts;
  list<evento_t, shm_Allocator<evento_t> >tout_red_aplic;
  list<evento_t, shm_Allocator<evento_t> >temporizadores_libres;

  // semaforo
  // lista temporizadores
  // lista de buf. libres
  // ...

  conexion_t CXs[NUM_MAX_CXs];
} kernel_shm_t;

/////////////////////////////////////////////////////////////////////
//  A partir de aquí podéis poner los prototipos de las funciones  //
//      que después implementaréis en el FUNCP.C                   //
/////////////////////////////////////////////////////////////////////

void ejemplo(int argumento);
uint16_t buscar_puerto_libre();
int comprobar_parametros(const t_direccion *tsap_destino,t_direccion *tsap_origen,char tipo);
int buscar_celda_libre();
void crear_pkt(tpdu *pkt,char tipo, t_direccion *tsap_dest,t_direccion *tsap_origen, void *datos, int longitud,int id_local, int id_destino);
int asign_conexion_CR(struct in_addr ip_remota, tpdu* puntero_pkt);
void inicializar_CXs_libres();
int buscar_connect_repetido(t_direccion *tsap_origen,const t_direccion *tsap_destino);
int buscar_listen_repetido(t_direccion *tsap_escucha, t_direccion *tsap_remota);
list<buf_pkt, shm_Allocator<buf_pkt> >::iterator buscar_buffer_libre();
list<evento_t, shm_Allocator<evento_t> >::iterator buscar_temporizador_libre();
uint32_t tiempo_actual();
void recalcular_temporizador_red(int id);
void recalcular_temporizador_aplic(int id);
int calcular_shortest();
void comprobar_vencimientos();


#endif /* _CONFP_H */
