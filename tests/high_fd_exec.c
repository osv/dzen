#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

enum { SKIP_STATUS = 77 };

int main(int argc, char **argv) {
    int fd;

    if (argc < 2) {
        fprintf(stderr, "usage: %s PROGRAM [ARG ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    for (;;) {
        fd = open("/dev/null", O_RDONLY);
        if (fd < 0) {
            if (errno == EMFILE || errno == ENFILE)
                return SKIP_STATUS;
            perror("high_fd_exec: open");
            return EXIT_FAILURE;
        }
        if (fd >= FD_SETSIZE)
            break;
    }

    execv(argv[1], &argv[1]);
    perror("high_fd_exec: execv");
    return EXIT_FAILURE;
}
