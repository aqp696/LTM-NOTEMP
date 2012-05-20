#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>   
#include <time.h> 
#include <sys/stat.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>

#include "ltmdaemon.h"
#include "ltmtypes.h"
#include "ltmipcs.h"

#include "comunicacion_privado.h"

kernel_shm_t * KERNEL = NULL;

char dir_proto[64] = KERNEL_MEM_ADDR;
int NUM_BUF_PKTS = 100;
int pe = 0;
int RTT = 0;
struct in_addr IP_retardo;
char interfaz[8] = "eth0";
int LTM_PROTOCOL = 140;

/////////////////////////////////////////////////////////////////

static int pfd[2];

static void RI_ctrC(int signum) {
    libera_recursos();
    exit(1);
}

static void RI_daemon(int signum) {
    signal(SIGUSR1, RI_daemon);

    if (write(pfd[1], "A", 1) < 0)
        perror("write");

    // printf("dentro RI_daemon\n");
}

///////////////////////////////////////////////////////////////////
static void chequeo_inicial(const char * mem_addr);
static void inicia_kernel();
static int inicia_ired();
//////////////////////////////////////////////////////////////////

static fd_set d_in_fds;
static int proto_socket;

int inicia_protocolo() {

    if (pipe(pfd) == -1) {
        perror("error en pipe: ");
        exit(-1);
    }

    chequeo_inicial(dir_proto);
    inicia_kernel();
    inicia_ired();

    //  Preparamos el conjunto de descriptores donde aguardaremos
    //  la llegada de eventos

    FD_ZERO(&d_in_fds);
    FD_SET(proto_socket, &d_in_fds);
    FD_SET(pfd[0], &d_in_fds);

    return (EXOK);
}

////////////////////////////////////////////////////////////////

static void chequeo_inicial(const char* mem_addr) {

    // Chequeamos si hay ya un protocolo ejecutandose
    // o si hay problemas para crear la memoria compartida

    int size = 0, res = -1;

    res = open(mem_addr, O_CREAT | O_EXCL, S_IRWXU);
    if ((res == -1) && (errno == EEXIST))
        goto abortar;
    close(res);

    res = shmget(ftok(mem_addr, 17), size, 0);
    if ((res == -1) && (errno == ENOENT))
        return;
    else if (res > 0) {

    } else
        perror("En chequeo_inicial: ");

abortar:
    fprintf(stderr, "Atención: asegúrate de que no haya otro protocolo ejecutándose o\n"
            "elimina (a mano) la memoria compartida no liberada correctamente en\n"
            "una ejecución anterior\n");

    exit(-1);
}

//////////////////////////////////////////////////////////////////

static void inicia_kernel() {

    int t_km = sizeof (kernel_shm_t);

    int er = CreaMemoriaKERNEL(dir_proto, t_km, (void**) & KERNEL);

    if (er < 0) exit(-1);

    KERNEL->kernel_pid = getpid();
    //KERNEL->GBTx.num_buf_pkts = NUM_BUF_PKTS;
    //KERNEL->GBRx.num_buf_pkts = NUM_BUF_PKTS + 1;
}

static int inicia_ired() {
    interfaz_red_interno_t *iri = (interfaz_red_interno_t*)&(KERNEL->i_red);
    iri->pe = pe;
    iri->RTT = RTT;
    iri->IP_retardo = IP_retardo;
    strcpy(KERNEL->i_red.interfaz, interfaz);
    KERNEL->i_red.LTM_PROTOCOL = LTM_PROTOCOL;    

    int er = inicia_interfaz_red(iri, &proto_socket);

    if (er < 0) {
        libera_recursos();
        exit(-1);
    }

    // semilla para el generador de numeros aleatorios
    srand(time(0));

    signal(SIGINT, RI_ctrC);
    signal(SIGUSR1, RI_daemon);

    return 0;
}


////////////////////////////////////////////////////////////////////////////

evento ltm_wait4event(int timeout) {
    char buf[100];

    if (timeout < 0)
        timeout = POLL_FREQ;
    else
        timeout = (timeout < POLL_FREQ) ? timeout : POLL_FREQ;

    struct timeval time_out;
    time_out.tv_sec = (long) (timeout / 1000);
    time_out.tv_usec = (long) ((timeout % 1000) * 1000);

    fd_set test_fds = d_in_fds;

    int n = select(32, &test_fds, 0, 0, &time_out);

    //  If n == 0, timeout
    if (n == 0)
        return TIME_OUT;
    else if ((n < 0) && (errno == EINTR)) {
        // printf("suenho interrumpido \n");
        if (read(pfd[0], buf, 100) < 0)
            perror("read");
        return INTERRUP;
    } else if (FD_ISSET(pfd[0], &test_fds)) {
        // printf("interrumpido en movimiento\n");
        if (read(pfd[0], buf, 100) < 0)
            perror("read");
        return INTERRUP;
    }

    return PAQUETE;
}

////////////////////////////////////////////////////////////////////////////

void libera_recursos(void) {
    unlink(dir_proto);
    LiberaMemoriaKERNEL((void**) & KERNEL);
    fprintf(stderr, "protocolo interrumpido\n");
}

