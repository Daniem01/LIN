#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#define __NR_LEDCTL 452

int main(int argc, char *argv[]) {
    if(argc != 2){
        fprintf(stderr, "Uso: %s <mascara>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Confirmamos que introducen un numero
    char *fin;
    long val = strtol(argv[1], &fin, 0);

    if (fin == argv[1] || *fin != '\0') {
        fprintf(stderr, "%s no es un numero\n", argv[1]);
        return EXIT_FAILURE;
    }
    // COnfirmamos que sea un numero valido
    if (val < 0 || val > 7) {
        fprintf(stderr, "%s esta fuera del rango (0-7)\n", argv[1]);
        return EXIT_FAILURE;
    }

    long ret = syscall(__NR_LEDCTL, val);

    if (ret != 0) {
        perror("ledctl");
        return EXIT_FAILURE;
    }
    return 0;
}