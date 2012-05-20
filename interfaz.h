#ifndef _INTERFAZ_H
#define _INTERFAZ_H

#include "ltmtypes.h"

#ifdef	__cplusplus
extern "C" {
#endif

    extern char dir_proto[64];

    int t_connect(const t_direccion *tsap_destino, t_direccion *tsap_origen);
    int t_listen(t_direccion *tsap_escucha, t_direccion *tsap_remota);
    int t_disconnect(int id);
    int t_flush(int id);
    size_t t_send(int id, const void *datos, size_t longitud, int8_t *flags);
    size_t t_receive(int id, void *datos, size_t longitud, int8_t *flags);

#ifdef	__cplusplus
}
#endif


#endif /* _INTERFAZ_H */
