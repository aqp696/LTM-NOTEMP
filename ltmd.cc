#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "ltmdaemon.h"

int main(int argc, char **argv) {

    char mi_maquina[128];
    gethostname(mi_maquina, 128);
    time_t t = time(0);


    if (procesa_argumentos(argc, argv) == EXOK) {
        printf("Lanzado ltmd (%d) on ", getpid());
        printf("%s %s", mi_maquina, ctime(&t));

        if (inicia_protocolo() != EXOK) {
            fprintf(stderr, " Error en la iniciacion de ltmd\n");
            exit(1);
        }

        bucle_principal();
        libera_recursos();
    }

    return 0;
}
