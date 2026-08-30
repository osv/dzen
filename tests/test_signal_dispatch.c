/* Unit tests for delivery, select wakeups, coalescing, and cleanup. */
#include "signal_dispatch.h"
#include "test_common.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

static void check_disposition(int signum, void (*expected)(int)) {
    struct sigaction action;

    CHECK(sigaction(signum, NULL, &action) == 0);
    CHECK(action.sa_handler == expected);
}

static void set_disposition(int signum, void (*handler)(int)) {
    struct sigaction action = { 0 };

    action.sa_handler = handler;
    CHECK(sigemptyset(&action.sa_mask) == 0);
    CHECK(sigaction(signum, &action, NULL) == 0);
}

static void check_handler_installed(int signum) {
    struct sigaction action;

    CHECK(sigaction(signum, NULL, &action) == 0);
    CHECK(action.sa_handler != SIG_DFL);
    CHECK(action.sa_handler != SIG_IGN);
}

/* Raise one signal and verify that it wakes select() and is reported once. */
static void check_signal(SignalDispatch *dispatch, int signum, unsigned int expected) {
    fd_set         read_fds;
    struct timeval timeout = { 1, 0 };
    int            fd      = signal_dispatch_fd(dispatch);

    CHECK(raise(signum) == 0);
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    CHECK(select(fd + 1, &read_fds, NULL, NULL, &timeout) == 1);
    CHECK(FD_ISSET(fd, &read_fds));
    CHECK(signal_dispatch_take(dispatch) == expected);
    CHECK(signal_dispatch_take(dispatch) == 0);
}

static void check_default_termination(int signum) {
    pid_t child = fork();
    int   status;

    CHECK(child >= 0);
    if (child == 0) {
        CHECK(raise(signum) == 0);
        _exit(EXIT_SUCCESS);
    }
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFSIGNALED(status));
    CHECK(WTERMSIG(status) == signum);
}

static void check_fd_flags(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL);
    CHECK(flags >= 0);
    CHECK((flags & O_NONBLOCK) != 0);
    flags = fcntl(fd, F_GETFD);
    CHECK(flags >= 0);
    CHECK((flags & FD_CLOEXEC) != 0);
}

static void run_configuration(int handle_usr1, int handle_usr2) {
    SignalDispatch dispatch;
    unsigned int   expected;
    int            read_fd;
    int            write_fd;

    /* Make the optional-handler checks independent of the invoking shell. */
    set_disposition(SIGUSR1, SIG_DFL);
    set_disposition(SIGUSR2, SIG_DFL);
    CHECK(signal_dispatch_init(&dispatch, handle_usr1, handle_usr2) == 0);
    read_fd  = dispatch.read_fd;
    write_fd = dispatch.write_fd;
    CHECK(signal_dispatch_fd(&dispatch) == read_fd);
    check_fd_flags(read_fd);
    check_fd_flags(write_fd);

    check_handler_installed(SIGTERM);
    check_handler_installed(SIGALRM);
    if (handle_usr1)
        check_handler_installed(SIGUSR1);
    else
        check_disposition(SIGUSR1, SIG_DFL);
    if (handle_usr2)
        check_handler_installed(SIGUSR2);
    else
        check_disposition(SIGUSR2, SIG_DFL);

    check_signal(&dispatch, SIGTERM, SIGNAL_DISPATCH_TERM);
    check_signal(&dispatch, SIGALRM, SIGNAL_DISPATCH_ALRM);
    if (handle_usr1)
        check_signal(&dispatch, SIGUSR1, SIGNAL_DISPATCH_USR1);
    else
        check_default_termination(SIGUSR1);
    if (handle_usr2)
        check_signal(&dispatch, SIGUSR2, SIGNAL_DISPATCH_USR2);
    else
        check_default_termination(SIGUSR2);

    expected = SIGNAL_DISPATCH_TERM | SIGNAL_DISPATCH_ALRM;
    CHECK(raise(SIGTERM) == 0);
    CHECK(raise(SIGTERM) == 0);
    CHECK(raise(SIGALRM) == 0);
    CHECK(raise(SIGALRM) == 0);
    if (handle_usr1) {
        CHECK(raise(SIGUSR1) == 0);
        CHECK(raise(SIGUSR1) == 0);
        expected |= SIGNAL_DISPATCH_USR1;
    }
    if (handle_usr2) {
        CHECK(raise(SIGUSR2) == 0);
        CHECK(raise(SIGUSR2) == 0);
        expected |= SIGNAL_DISPATCH_USR2;
    }
    CHECK(signal_dispatch_take(&dispatch) == expected);
    CHECK(signal_dispatch_take(&dispatch) == 0);

    signal_dispatch_shutdown(&dispatch);
    CHECK(dispatch.read_fd == -1);
    CHECK(dispatch.write_fd == -1);
    errno = 0;
    CHECK(fcntl(read_fd, F_GETFD) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(fcntl(write_fd, F_GETFD) == -1);
    CHECK(errno == EBADF);

    check_disposition(SIGTERM, SIG_IGN);
    check_disposition(SIGALRM, SIG_IGN);
    check_disposition(SIGUSR1, handle_usr1 ? SIG_IGN : SIG_DFL);
    check_disposition(SIGUSR2, handle_usr2 ? SIG_IGN : SIG_DFL);
}

static void check_configuration(int handle_usr1, int handle_usr2) {
    pid_t child = fork();
    int   status;

    CHECK(child >= 0);
    if (child == 0) {
        run_configuration(handle_usr1, handle_usr2);
        _exit(EXIT_SUCCESS);
    }
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == EXIT_SUCCESS);
}

int main(void) {
    check_configuration(0, 0);
    check_configuration(1, 0);
    check_configuration(0, 1);
    check_configuration(1, 1);

    puts("signal dispatcher tests passed");
    return EXIT_SUCCESS;
}
