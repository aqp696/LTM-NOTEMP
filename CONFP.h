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

#define CLOSED 201
#define LISTEN 202
#define CONNECT 203
#define ESTABLISHED 204

#define DEPURA 0

#define tiempo_rtx_pkt tiempo_actual()-KERNEL->t_inicio+200
#define tiempo_rtx_red tiempo_actual() -KERNEL->t_inicio+1000
#define tiempo_rtx_aplic tiempo_actual() -KERNEL->t_inicio+1000
#define NUM_MAX_RTx 6

//ESTADOS DE ENVIO DE PKT
#define no_confirmado 1
#define confirmado 2

#define vencimiento_pkt 300
#define vencimiento_red 301
#define vencimiento_aplic 302


//const int CR = 101;
//const int CC = 102;
//
////DEFINICION DE ESTADOS DE CONEXION
//const int CLOSED = 201;
//const int LISTEN = 202;
//const int CONNECT = 203;
//const int ESTABLISHED = 204;


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
    int tipo; //CC o CR
    int id_local;
    int id_destino;
    uint16_t puerto_orig;
    uint16_t puerto_dest;
    struct in_addr ip_local;
    struct in_addr ip_destino;
    int conexion_aceptada;
    int tamanho_datos;
    int num_seq_ack;
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
    int num_secuencia;
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
  struct in_addr ip_destino;
  struct in_addr ip_local;
  
  bool primitiva_dormida;
  int resultado_primitiva;//aki es donde ponemos si fue un EXNET,OK ...
  // semaforo?
  // lista de paquetes/buf. pendientes de asentimiento
  // lista de paquetes/buf. recibidos
  // ...
  
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
  list<evento_t, shm_Allocator<evento_t> >::iterator it_tipo_vencimiento;
  list<buf_pkt, shm_Allocator<buf_pkt> >buffers_libres;
  list<int, shm_Allocator<int> >CXs_libres;
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
int asign_conexion_CR(tpdu *,kernel_shm_t *);
void inicializar_CXs_libres();
int buscar_connect_repetido(t_direccion *tsap_origen,const t_direccion *tsap_destino);
int buscar_listen_repetido(t_direccion *tsap_escucha, t_direccion *tsap_remota);
list<buf_pkt>::iterator buscar_buffer_libre();

#endif /* _CONFP_H */
