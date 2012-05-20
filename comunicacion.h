///////////////////////////////////////////////////////
// Este fichero de cabecera contiene los prototipos
// de las funciones del interfaz con el nivel de red
// para el desarrollo de programas que deseen utilizar
// la comunicacion a traves de dicho nivel.
//////////////////////////////////////////////////////

#ifndef COMUNICACION__H
#define COMUNICACION__H

#include <stddef.h>
#include <netinet/in.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef union _interfaz_red {
    struct {
        char interfaz[8];
        struct in_addr ip_local;
        int LTM_PROTOCOL;
    };
    char datos_ir[64];
} interfaz_red_t;

int recibir_tpdu(void *bufer_pkt, size_t longitud, struct in_addr * ip_fuente, unsigned int *offset);
int enviar_tpdu(struct in_addr ipdest, const void *tpdu, size_t longitud);
int obtener_IP_local(const char *if_name, struct in_addr *ip);

#ifdef	__cplusplus
}
#endif

#endif


















