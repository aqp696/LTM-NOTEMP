#include "comunicacion.h"
#include "comunicacion_privado.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>  
#include <unistd.h> 
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <inttypes.h>
#include <ifaddrs.h>

#include <sys/msg.h>
#include <sys/ipc.h>
#include <signal.h>

static interfaz_red_interno_t * IR = NULL;
static int PROTO_SOCKET;
static uint64_t recv_bytes, sent_bytes;
static int count_bytes = 0;

//////////////////////////////////////////////////////////
static int get_fd(int ltm_protocol);
//////////////////////////////////////////////////////////

static void dump_info() {
    fprintf(stderr, "Bytes enviados: %"PRIu64", recibidos: %"PRIu64"\n", sent_bytes, recv_bytes);
}

int inicia_interfaz_red(void * p, int *protoS) {
    struct sockaddr_in sockin;
    char *debug_bytes;

    debug_bytes = getenv("_LTM_DEBUG");
    if (debug_bytes && atoi(debug_bytes)) {
        count_bytes = 1;
        recv_bytes = sent_bytes = 0;
        atexit(dump_info);
    }

    IR = (interfaz_red_interno_t*) p;

    PROTO_SOCKET = get_fd(IR->LTM_PROTOCOL);
    if (PROTO_SOCKET < 0) {
        printf("%d error i_red proto_socket\n", errno);
        perror(0);
        return -1;
    }
    *protoS = PROTO_SOCKET;

    if (obtener_IP_local(IR->interfaz, &IR->ip_local) == 0) {
        printf("Problemas al obtener la ip local para %s\n", IR->interfaz);
        return -1;
    }

    memset(&sockin, 0, sizeof (struct sockaddr_in));

    sockin.sin_family = AF_INET;
    sockin.sin_port = 0;
    sockin.sin_addr = IR->ip_local;

    if (bind(PROTO_SOCKET, (struct sockaddr *) &sockin,
            sizeof (struct sockaddr_in)) == -1) {
        perror("error: bind en inica_IR");
        return -1;
    };

    return 0;
}


//////////////////////////////////////////////////////////

int config_interfaz_red(void *p) {
    IR = (interfaz_red_interno_t*) p;

    PROTO_SOCKET = get_fd(IR->LTM_PROTOCOL);
    if (PROTO_SOCKET < 0) {
        perror("Error al configurar convergencia: ");
        return (-1);
    }

    struct sockaddr_in sockin;
    memset(&sockin, 0, sizeof (struct sockaddr_in));

    sockin.sin_family = AF_INET;
    sockin.sin_port = 0;
    sockin.sin_addr = IR->ip_local;

    if (bind(PROTO_SOCKET, (struct sockaddr *) &sockin,
            sizeof (struct sockaddr_in)) == -1) {
        perror("error: bind en inica_convergencia");
        exit(-1);
    };

    srand(time(0));
    return (0);
}

////////////////////////////////////////////////
//            RECIBIR_TPDU
////////////////////////////////////////////////

int recibir_tpdu(void *bufer_pkt, size_t longitud, struct in_addr *ip_fuente, unsigned int *offset) {
    int recibidos,len_cab_ip;
    struct iphdr *iphdrs = (struct iphdr *) bufer_pkt;
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr_in);

    recibidos = recvfrom(PROTO_SOCKET, (char *) bufer_pkt, longitud, 0,
			 (struct sockaddr *)&addr, &len);
    len_cab_ip= 4 * iphdrs->ihl;

    if (recibidos < 0) {
      perror("recvfrom");
      return recibidos;
    } else if (recibidos < len_cab_ip) {
      printf("error inesparado en recvfrom: cabecera ip incompleta");
      return -1;
    }

    memset((char *)bufer_pkt,0,len_cab_ip);
    *offset=len_cab_ip;
    
    if (count_bytes)
        recv_bytes += recibidos-len_cab_ip;

    memcpy(ip_fuente, &addr.sin_addr, sizeof(struct in_addr));
    
    return recibidos-len_cab_ip;
}



////////////////////////////////////////////////
//            ENVIAR_TPDU
////////////////////////////////////////////////

/* Un algoritmo generador pseudoaleatorio sencillo, pero suficiente */
uint32_t _get_random(void) {
    static int m_w = 1, m_z = 1;

    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) +m_w; /* 32-bit result */
}

int enviar_tpdu(struct in_addr ipdest, const void *tpdu, size_t longitud) {
    struct sockaddr_in sock_dir;
    char *paquete = (char *) tpdu;
    int enviados;
 
    if (longitud > 1500)
        return -1;
    // maxima longitud del campo de datos de una trama ethernet

    if (IR->pe > 0) {
        int j = _get_random() % 100;

        if (j <= IR->pe)
            return longitud;
    }

    memset((char*) & sock_dir, 0, sizeof (struct sockaddr_in));
    sock_dir.sin_family = AF_INET;
    sock_dir.sin_addr = ipdest;
    sock_dir.sin_port = 0;

    int X = 4;
    if (IR->RTT != 0) {
        char encapsulado[1500]; // longitud + X <= 1514-20-14
        memcpy(encapsulado + X, paquete, longitud);
        paquete = encapsulado;
        longitud += X;
        memcpy(paquete, &(sock_dir.sin_addr.s_addr), sizeof (struct in_addr));
        sock_dir.sin_addr = IR->IP_retardo;
    }

    enviados = sendto(PROTO_SOCKET, paquete, longitud, 0,
            (struct sockaddr *) & sock_dir, sizeof (sock_dir));
    if (enviados < 0) {
        perror("enviar_pkt: ");
    }

    if (count_bytes)
        sent_bytes += enviados;

    return (IR->RTT) ? longitud - X : longitud;

}

////////////////////////////////////////////////
//          OBTENER_IP_LOCAL
////////////////////////////////////////////////

int obtener_IP_local(const char *if_name, struct in_addr *ip) {
    struct ifaddrs *ifaddr, *ifa;
    int family;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return 0;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && strcmp(ifa->ifa_name, if_name) == 0) {
            family = ifa->ifa_addr->sa_family;
            if (family == AF_INET) {
                *ip = ((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr;
	            break;
            }
        }
    }
    freeifaddrs(ifaddr);
    return 1;
}

////////////////////////////////////////////////////////////////

#define GET_FD "/usr/local/sbin/ltm_getRAWfd"

static int get_fd(int ltm_protocol) {
    int fd[2], new_fd = -1;
    pid_t child;

    socketpair(PF_UNIX, SOCK_DGRAM, 0, fd);
    if ((child = fork()) == 0) {
        char buf[8];
        char buf2[8];
        sprintf(buf, "%d", fd[0]);
        sprintf(buf2, "%d", ltm_protocol);
        fcntl(fd[0], F_SETFD, fcntl(fd[0], F_GETFD) & !FD_CLOEXEC);
        fcntl(fd[1], F_SETFD, fcntl(fd[1], F_GETFD) & !FD_CLOEXEC);
        execl(GET_FD, GET_FD, buf, buf2, (char *) NULL);
    } else {
        /* Receive msg and wait */
        struct msghdr msgh = {0};
        struct cmsghdr *cmsg;
        char buf[1024];
        //size_t len;
        int len;

        //    fprintf (stderr, "Esperando mensaje.\n");
        msgh.msg_control = buf;
        msgh.msg_controllen = sizeof (buf);

        if ((len = recvmsg(fd[1], &msgh, 0)) < 0) {
            perror("recvmsg");
        } else {
            //      fprintf (stderr, "Recibido mensaje de longitud: %d\n", len);
            for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL;
                    cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
                if (cmsg->cmsg_level == SOL_SOCKET &&
                        cmsg->cmsg_type == SCM_RIGHTS) {
                    //new_fd = *((int *) CMSG_DATA(cmsg));
                    memcpy(&new_fd, CMSG_DATA(cmsg), sizeof (new_fd));
                    break;
                } else {
                    fprintf(stderr, "Tipo incorrecto.\n");
                }
            }
        }
    }

    waitpid(child, NULL, 0);
    close(fd[0]);
    close(fd[1]);

    return new_fd;
}














