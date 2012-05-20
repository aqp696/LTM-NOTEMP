#ifndef _LTM_IPC_H
#define _LTM_IPC_H

#include <unistd.h>
#include <pthread.h>

#ifdef	__cplusplus
extern "C" {
#endif

    int CreaMemoriaKERNEL(const char *dir_proto, int tamanho, void **p1);
    int ltm_get_kernel(const char *dir_proto, void **p1);
    void ltm_exit_kernel(void **p1);
    void LiberaMemoriaKERNEL(void **p1);

    void* alloc_kernel_mem(int tam);
    void free_kernel_mem(void *p1);

    void interrumpe_daemon();

    pid_t gettid(); // devuelve el identificador del thread (proceso)

    ////////////////////////////////////////////////////////
    // BARRERAS
    ////
    //  Una barrera es un punto de encuentro para un conjunto
    //  de procesos (en nuestro caso 2: el proceso LTMdaemon y
    //  la aplicación "cuando es transporte" dentro de una llamada
    //	     al sistema)

    //  La barrera se configura internamente asociándole un NÚMERO, y cuando
    //  un proceso llega a ella espera a que se junten allí el NÚMERO
    //  de ellos con que fue configurada inicialmente. Cuando llegue
    //  el que completa la agrupación todos pueden proseguir su ejecución.
    //
    //  bloquea_llamada y despierta_conexion son realmente funciones
    //  idénticas que representan la llegada a una barrera de una
    //  llamada al sistema (bloquea_llamada) o del proceso "demonio"
    //  (despierta_conexion)
    //
    //    inicia_barrera
    //  Permite configurar una barrera "barr" definida en una zona
    // de memoria compartida.
    //
    /////////////////////////////////////////////////////////////////////////////

    typedef pthread_barrier_t barrera_t;

    int inicia_barrera(barrera_t *barrera);
    int bloquea_llamada(barrera_t *barr);
    int despierta_conexion(barrera_t *barr);

    ////////////////////////////////////////////////////////
    // CONTROL DEL ACCESO A DATOS CONCURRENTEMENTE
    ////
    //     INICIA_SEMAFORO
    //  Permite configurar un semáforo "a" definido en una zona
    // de memoria compartida. Inicialmente estará verde.
    //
    //      BLOQUEAR_ACCESO
    //  Permite bloquear el acceso a una región crítica
    // mediante la variable "a" (cerrar el semáforo), antes
    // de acceder a los datos para los que se
    // desea garantizar su integridad en aquellas funciones
    // que accedan a estos de modo concurrente.
    //
    //      DESBLOQUEAR_ACCESO
    //  Permite desbloquear el acceso a una región crítica
    // mediante la variable "a" (abrir el semáforo), tras
    // acceder a los datos para los que se
    // desea garantizar su integridad en aquellas funciones
    // que accedan a estos de modo concurrente.
    //
    //////////////////////////////////////////////////////

    typedef pthread_mutex_t semaforo_t;
    int inicia_semaforo(semaforo_t *mutex);

    int bloquear_acceso(semaforo_t *a);
    int desbloquear_acceso(semaforo_t *a);

#ifdef	__cplusplus
}
#endif

#endif /* _LTM_IPC_H */
