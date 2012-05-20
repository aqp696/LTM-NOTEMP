#ifndef _LTM_TYPES_H
#define _LTM_TYPES_H

#include <stdint.h>
#include <netinet/in.h>

//// TSAPs  //////

typedef struct {
  struct in_addr ip;
  int16_t puerto;
} t_direccion;


/// Flags

extern const int8_t CLOSE;            //  flujo de datos cerrado
extern const int8_t CLOSE_SEND;       //  el protocolo cierra el flujo de datos saliente

#define KERNEL_MEM_ADDR "s.ltmdg"  // fichero por defecto asociado a la memoria
                                   // compartida del kernel
#define MAX_DATOS   1024    // limite (en bytes) para los datos que
                            // puede transportar un paquete
#define NUM_MAX_CXs 8
#define CABECERA_IP 20

////////////////////////////////////////////////////////////////////////////
//
//  CODIGOS DE ERROR
//
////////////////////////////////////////////////////////////////////////////

// Okey

extern const int EXOK;         // Okay

// Errores

extern const int EXBADTID;     // La conexion referida no esta en la tabla
extern const int EXCDUP;       // Colision con una conexion ya presente en la tabla
extern const int EXNET;        // Error de (sub)red
extern const int EXKERNEL;     // Error al entrar en el kernel
extern const int EXMAXDATA;    // Longitud maxima de TPDU excedida
extern const int EXNOTSAP;     // No existe el TSAP especificado
extern const int EXNODATA;     // Datos no disponibles
extern const int EXCLOSE;      // Flujo de datos ya cerrado
extern const int EXINVA;       // Argumento invalido
extern const int EXMAXC;       // No se admiten mas conexiones

extern const int EXUNDEF;      // error indefinido
extern const int EXDISC;

#endif /* _LTM_TYPES_H */
