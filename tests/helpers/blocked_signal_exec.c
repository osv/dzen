#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    sigset_t mask;

    if (argc < 2) {
        fprintf(stderr, "usage: %s PROGRAM [ARG ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (sigemptyset(&mask) < 0 || sigaddset(&mask, SIGTERM) < 0 || sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        perror("blocked_signal_exec: sigprocmask");
        return EXIT_FAILURE;
    }

    execv(argv[1], &argv[1]);
    perror("blocked_signal_exec: execv");
    return EXIT_FAILURE;
}
