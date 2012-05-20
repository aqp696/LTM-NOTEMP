/* 
 * File:   ltmtypes.c
 * Author: miguel
 *
 * Created on 3 de febrero de 2010, 15:36
 */

#include "ltmtypes.h"

/// MODOS

const int8_t CLOSE         = 0X01;
const int8_t CLOSE_SEND    = 0x02;
const int8_t RECV_READY    = 0x04;
const int8_t SEND_READY    = 0x08;

////////////////////////////////////////////////////////////////////////////
//
//  CODIGOS DE ERROR
//
////////////////////////////////////////////////////////////////////////////

// Okay

const int EXOK    =   0;   // Okay

// Errores

const int EXBADTID   = -1;   // La conexion referida no esta en la tabla
const int EXCDUP     = -2;   // Colision con una conexion ya presente en la tabla
const int EXNET      = -3;   // Error de (sub)red
const int EXKERNEL   = -4;   // Error al entrar en el kernel
const int EXMAXDATA  = -5;   // Longitud maxima de TPDU excedida
const int EXNOTSAP   = -6;   // No existe el TSAP especificado
const int EXNODATA   = -7;   // Datos no disponibles
const int EXCLOSE    = -8;   // Flujo de datos ya cerrado
const int EXINVA     = -9;   // Argumento invalido
const int EXMAXC     = -10;  // No se admiten mas conexiones
const int EXUNDEF    = -11;  // error indefinido
const int EXDISC     = -12;
