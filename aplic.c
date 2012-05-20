#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//esto es cosa mia
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "comunicacion.h"

#include "interfaz.h"
#include "ltmtypes.h"

int main(int argc, char **argv) {

    int opt;
    char *dir = NULL;
    char *puerto = NULL;
    int iPuerto = 0;
    int flagServidor=0;
    int resultado_primitiva;
    t_direccion tsap_destino, tsap_origen;

    memset(&tsap_origen, 0, sizeof (tsap_origen));
    memset(&tsap_destino, 0, sizeof (tsap_destino));

    while ((opt = getopt(argc, argv, "scp:r:")) != -1) {
        switch (opt) {
            case 's':
                fprintf(stderr,"\naplicacion servidor\n");
                flagServidor = 1;
                break;
            case 'c':
                fprintf(stderr,"\naplicacion cliente\n");
                flagServidor = 0;
                break;
            case 'p':
                puerto = optarg;
                iPuerto = atoi(puerto);
                fprintf(stderr,"\nEl puerto es: %d\n", iPuerto);
                //tsap_origen.puerto = iPuerto;
                break;
            case 'r':
                dir = optarg;
                inet_pton(AF_INET, dir, &tsap_destino.ip);
                fprintf(stderr,"\nla direccion es: %s\n", dir);
                break;
        }
    }

    //miramos si la aplicacion funciona como cliente o como servidor
    if(flagServidor == 0){
        tsap_destino.puerto=iPuerto;
        resultado_primitiva = t_connect (&tsap_destino, &tsap_origen);
    }else{
        tsap_origen.puerto = iPuerto;
        resultado_primitiva = t_listen (&tsap_origen, &tsap_destino);
    }

    //resultados de la primitiva
    switch(resultado_primitiva){
        case 0:
            fprintf(stderr,"\nPrimitiva ejecutada con exito\n");
            break;
        case -1:
            fprintf(stderr,"\nLa conexion referida no esta en la tabla\n");
            break;
        case -2:
            fprintf(stderr,"\nColision con una conexion ya presente en la tabla\n");
            break;
        case -3:
            fprintf(stderr,"\nError de (sub)red.Agotado el numero de retransmisiones de un paquete\n");
            break;
        case -4:
            fprintf(stderr,"\nError al entrar en el kernel\n");
            break;
        case -5:
            fprintf(stderr,"\nMensaje demasiado grande\n");
            break;
        case -6:
            fprintf(stderr,"\nNo existe el TSAP especificado\n");
            break;
        case -7:
            fprintf(stderr,"\nDatos no disponibles\n");
            break;
        case -8:
            fprintf(stderr,"\nFlujo de datos ya cerrado\n");
            break;
        case -9:
            fprintf(stderr,"\nArgumento invalido\n");
            break;
        case -10:
            fprintf(stderr,"\nNumero maximo de conexiones alcanzado\n");
            break;
        case -11:
            fprintf(stderr,"\nError indefinido\n");
            break;
        case -12:
            fprintf(stderr,"\nRecibida desconexion\n");
            break;
    }
   
    return 0;
}
