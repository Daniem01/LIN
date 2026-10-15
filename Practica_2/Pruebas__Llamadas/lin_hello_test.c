#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#define __NR_HELLO 451

int main(void) {
    long ret = syscall(__NR_HELLO);

    if (ret != 0) {
        perror("lin_hello");
        return EXIT_FAILURE;
    }
    return 0;
}