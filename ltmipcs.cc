#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <pthread.h>

#include "ltmtypes.h"
#include "ltmipcs.h"
#include "ltmallocator.h"

#include "pool.h"

#include "comunicacion_privado.h"

#define SHM_BASE_ADDR (void*)0X40a00000
#define LTM_HEAP 500000

typedef struct {
    int offset;
    pthread_mutex_t heap_sema;
    pid_t daemon_pid;
    pool heap_pool;
} shm_cntrl;

pthread_mutex_t * shm_Allocator_base::sema = NULL;
pool * shm_Allocator_base::memory_pool = NULL;


#define INICIO sizeof(shm_cntrl)

static int shm_id1 = -1;
static char * shm_base_addr = NULL;
static shm_cntrl * shmc = NULL;

int CreaMemoriaKERNEL(const char * dir_proto, int tamanho, void**p1) {
    int tam1 = (tamanho / 4)*4 + 4;

    shm_id1 = shmget(ftok(dir_proto, 17), INICIO + tam1 + LTM_HEAP,
            0600 | IPC_CREAT | IPC_EXCL);
    if (shm_id1 < 0) {
        perror("En CreaMemoriaKERNEL (shmget): ");
        printf("cuidado !!! posiblemente tengas que eliminar antes memoria no liberada\n");
        return -1;
    }

    shm_base_addr = (char *) shmat(shm_id1, SHM_BASE_ADDR, SHM_RND);
    if (((void *) shm_base_addr) == (void *) - 1) {
        perror("En CreaMemoriaKERNEL (shmat): ");
        return -1;
    }

    shmc = (shm_cntrl *) shm_base_addr;

    shmc->offset = tam1;

    *p1 = shm_base_addr + INICIO;

    shm_Allocator_base::sema = &(shmc->heap_sema);
    shm_Allocator_base::memory_pool = &(shmc->heap_pool);

    inicia_semaforo(&(shmc->heap_sema));

    memset((char*) * p1, 0, tam1 + LTM_HEAP);

    shmc->heap_pool = pool(LTM_HEAP, (char*) shm_base_addr + INICIO + tam1);

    shmc->daemon_pid = getpid();

    return 0;
};

static int iniciado = 0;

int ltm_get_kernel(const char * dir_proto, void**p1) {
    void *p2 = NULL;
    *p1 = NULL;
    int tam = 0;

    if (!iniciado) {
        shm_id1 = shmget(ftok(dir_proto, 17), tam, 0600);
        if (shm_id1 < 0) {
            perror("En entraKERNEL (shmget): ");
            printf("cuidado !!! posiblemente el kernel no ha sido creado todavia\n");
            return -1;
        }
    }

    shm_base_addr = (char *) shmat(shm_id1, SHM_BASE_ADDR, SHM_RND);
    if (((void *) shm_base_addr) == (void *) - 1) {
        perror("En entraKERNEL (shmat): ");
        return -1;
    }

    shmc = (shm_cntrl *) shm_base_addr;
    shm_Allocator_base::sema = &(shmc->heap_sema);
    shm_Allocator_base::memory_pool = &(shmc->heap_pool);

    *p1 = shm_base_addr + INICIO;
    p2 = shm_base_addr + INICIO;

    if (!iniciado) {
        int er = config_interfaz_red(p2);
        if (er == -1) {
            ltm_exit_kernel(p1);
            return -1;
        }
        iniciado = 1;
    }

    return 0;
};

void ltm_exit_kernel(void**p1) {

    if (shm_base_addr) {
        shmdt((void*) shm_base_addr);
        shm_base_addr = NULL;
    }
    *p1 = NULL;
}

void LiberaMemoriaKERNEL(void**p1) {

    ltm_exit_kernel(p1);
    if (shm_id1 > 0)
        shmctl(shm_id1, IPC_RMID, 0);
}



/////////////////////////////////////////////////////////////

void* alloc_kernel_mem(int tam) {
    char *p1;

    pthread_mutex_lock(&(shmc->heap_sema));
    p1 = (char *) shmc->heap_pool.allocate(tam);
    //shmc->heap_pool.dump();
    pthread_mutex_unlock(&(shmc->heap_sema));

    if (!p1)
        printf("memoria del kernel agotada\n");

    return p1;
}

void free_kernel_mem(void *p1) {
    pthread_mutex_lock(&(shmc->heap_sema));
    shmc->heap_pool.deallocate(p1);
    //shmc->heap_pool.dump();
    pthread_mutex_unlock(&(shmc->heap_sema));
}


/////////////////////////////////////////////////////////////

pid_t gettid(void) {
    return syscall(__NR_gettid);
}

static int tkill(pid_t tid, int sig) {
    return syscall(__NR_tkill, tid, sig);
}

static int espera_barrera(pthread_barrier_t *bar) {
    int err = pthread_barrier_wait(bar);
    if ((err < 0) && (err != PTHREAD_BARRIER_SERIAL_THREAD)) {
        perror("espera_barrera: ");
        return -1;
    }
    
    return 0;
}

int bloquea_llamada(barrera_t *barr) {
    return espera_barrera(barr);
}

int despierta_conexion(barrera_t *barr) {
    return espera_barrera(barr);
}

void interrumpe_daemon() {
    tkill(shmc->daemon_pid, SIGUSR1);
}

int inicia_semaforo(pthread_mutex_t * mutex) {

    pthread_mutexattr_t pshared;
    pthread_mutexattr_init(&pshared);
    pthread_mutexattr_setpshared(&pshared, PTHREAD_PROCESS_SHARED);

    int err = pthread_mutex_init(mutex, &pshared);
    if (err < 0)
        perror("init_sema: ");
    return err;
}

int bloquear_acceso(semaforo_t *a) {
    return pthread_mutex_lock(a);
}

int desbloquear_acceso(semaforo_t *a) {
    return pthread_mutex_unlock(a);
}

int inicia_barrera(pthread_barrier_t * barrera) {
    pthread_barrierattr_t pshared;

    int err = pthread_barrierattr_init(&pshared);
    if (err < 0) {
        perror("barrera 0");
        return -1;
    }
    err = pthread_barrierattr_setpshared(&pshared, PTHREAD_PROCESS_SHARED);
    if (err < 0) {
        perror("barrera 1");
        return -1;
    }

    int errr = pthread_barrier_init(barrera, &pshared, 2);
    if (errr != 0) {
        perror("barr: ");
        return -1;
    }

    return 0;
}
