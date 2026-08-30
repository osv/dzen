/* Unit tests for delivery, select wakeups, coalescing, and cleanup. */
#include "signal_dispatch.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

/* Raise one signal and verify that it wakes select() and is reported once. */
static void assert_signal(SignalDispatch *dispatch, int signum, unsigned int expected) {
    fd_set         read_fds;
    struct timeval timeout = { 1, 0 };
    int            fd      = signal_dispatch_fd(dispatch);

    assert(raise(signum) == 0);
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    assert(select(fd + 1, &read_fds, NULL, NULL, &timeout) == 1);
    assert(FD_ISSET(fd, &read_fds));
    assert(signal_dispatch_take(dispatch) == expected);
    assert(signal_dispatch_take(dispatch) == 0);
}

int main(void) {
    SignalDispatch dispatch;
    int            read_fd;
    int            write_fd;

    assert(signal_dispatch_init(&dispatch, 1, 1) == 0);
    read_fd  = dispatch.read_fd;
    write_fd = dispatch.write_fd;
    assert((fcntl(read_fd, F_GETFL) & O_NONBLOCK) != 0);
    assert((fcntl(write_fd, F_GETFL) & O_NONBLOCK) != 0);
    assert((fcntl(read_fd, F_GETFD) & FD_CLOEXEC) != 0);
    assert((fcntl(write_fd, F_GETFD) & FD_CLOEXEC) != 0);

    assert_signal(&dispatch, SIGTERM, SIGNAL_DISPATCH_TERM);
    assert_signal(&dispatch, SIGALRM, SIGNAL_DISPATCH_ALRM);
    assert_signal(&dispatch, SIGUSR1, SIGNAL_DISPATCH_USR1);
    assert_signal(&dispatch, SIGUSR2, SIGNAL_DISPATCH_USR2);

    assert(raise(SIGUSR1) == 0);
    assert(raise(SIGUSR1) == 0);
    assert(raise(SIGUSR1) == 0);
    assert(signal_dispatch_take(&dispatch) == SIGNAL_DISPATCH_USR1);

    signal_dispatch_shutdown(&dispatch);
    errno = 0;
    assert(fcntl(read_fd, F_GETFD) == -1 && errno == EBADF);
    errno = 0;
    assert(fcntl(write_fd, F_GETFD) == -1 && errno == EBADF);
    assert(raise(SIGTERM) == 0);
    assert(raise(SIGALRM) == 0);

    puts("signal dispatcher tests passed");
    return 0;
}
