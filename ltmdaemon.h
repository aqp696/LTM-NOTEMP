#ifndef _LTM_DAEMON_H
#define _LTM_DAEMON_H

#include "CONFP.h"

#define  POLL_FREQ 5000

#ifdef	__cplusplus
extern "C" {
#endif

    typedef enum {
        TIME_OUT,
        INTERRUP,
        PAQUETE
    } evento;

    //// VARIABLES DEL PROTOCOLO ////////////

    extern kernel_shm_t * KERNEL;
    extern int NUM_BUF_PKTS;

    ///// FUNCIONES ///////////////////
    int inicia_protocolo();
    evento ltm_wait4event(int timeout); // timeout en milisegundos
    void libera_recursos(void);

    int procesa_argumentos(int argc, char ** argv);
    void bucle_principal(void);

#ifdef	__cplusplus
}
#endif

#endif /* _LTM_DAEMON_H */

