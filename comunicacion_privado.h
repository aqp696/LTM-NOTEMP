/* 
 * File:   comunicacion_privado.h
 * Author: miguel
 *
 * Created on 3 de febrero de 2010, 12:55
 */

#ifndef _COMUNICACION_PRIVADO_H
#define	_COMUNICACION_PRIVADO_H

#include <netinet/in.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct _interfaz_red_interno {
        char interfaz[8];
        struct in_addr ip_local;
        int LTM_PROTOCOL;
        int pe;
        int RTT;
        struct in_addr IP_retardo;
    } interfaz_red_interno_t;

    int inicia_interfaz_red(void *p, int *protoS);
    int config_interfaz_red(void *p);

#ifdef	__cplusplus
}
#endif

#endif	/* _COMUNICACION_PRIVADO_H */

