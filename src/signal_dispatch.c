#include "signal_dispatch.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * Handlers only set sig_atomic_t flags and wake the event loop. All actions,
 * Xlib calls, and cleanup are deliberately left to normal process context.
 */
static volatile sig_atomic_t pending_term;
static volatile sig_atomic_t pending_alrm;
static volatile sig_atomic_t pending_usr1;
static volatile sig_atomic_t pending_usr2;
/* Zero means disabled; active descriptors are stored as fd + 1. */
static volatile sig_atomic_t signal_write_fd_code;

/* Async-signal-safe: preserve errno, update flags, and best-effort write. */
static void                  signal_handler(int signum) {
    int           saved_errno = errno;
    unsigned char byte        = (unsigned char)signum;
    ssize_t       write_result;

    switch (signum) {
    case SIGTERM:
        pending_term = 1;
        break;
    case SIGALRM:
        pending_alrm = 1;
        break;
    case SIGUSR1:
        pending_usr1 = 1;
        break;
    case SIGUSR2:
        pending_usr2 = 1;
        break;
    }

    if (signal_write_fd_code != 0) {
        int write_fd = (int)(signal_write_fd_code - 1);

        write_result = write(write_fd, &byte, sizeof(byte));
        (void)write_result;
    }
    errno = saved_errno;
}

static int set_fd_flags(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;
    flags = fcntl(fd, F_GETFD);
    if (flags < 0 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
        return -1;
    return 0;
}

static int install_handler(int signum) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return sigaction(signum, &action, NULL);
}

static void noop_handler(int signum) {
    (void)signum;
}

static void install_noop_handler(int signum) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = noop_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    (void)sigaction(signum, &action, NULL);
}

int signal_dispatch_init(SignalDispatch *dispatch, int handle_usr1, int handle_usr2) {
    int pipe_fds[2];

    dispatch->read_fd     = -1;
    dispatch->write_fd    = -1;
    dispatch->handle_usr1 = handle_usr1;
    dispatch->handle_usr2 = handle_usr2;
    sigemptyset(&dispatch->handled_signals);

    if (pipe(pipe_fds) < 0)
        return -1;
    if (set_fd_flags(pipe_fds[0]) < 0 || set_fd_flags(pipe_fds[1]) < 0) {
        int saved_errno = errno;
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        errno = saved_errno;
        return -1;
    }
    if ((uintmax_t)pipe_fds[1] + 1 > (uintmax_t)SIG_ATOMIC_MAX) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        errno = EMFILE;
        return -1;
    }

    dispatch->read_fd    = pipe_fds[0];
    dispatch->write_fd   = pipe_fds[1];
    signal_write_fd_code = (sig_atomic_t)((uintmax_t)dispatch->write_fd + 1);
    pending_term = pending_alrm = pending_usr1 = pending_usr2 = 0;

    sigaddset(&dispatch->handled_signals, SIGTERM);
    sigaddset(&dispatch->handled_signals, SIGALRM);
    if (handle_usr1)
        sigaddset(&dispatch->handled_signals, SIGUSR1);
    if (handle_usr2)
        sigaddset(&dispatch->handled_signals, SIGUSR2);

    if (install_handler(SIGTERM) < 0 || install_handler(SIGALRM) < 0 || (handle_usr1 && install_handler(SIGUSR1) < 0) ||
        (handle_usr2 && install_handler(SIGUSR2) < 0)) {
        int saved_errno = errno;
        signal_dispatch_shutdown(dispatch);
        errno = saved_errno;
        return -1;
    }
    return 0;
}

int signal_dispatch_fd(const SignalDispatch *dispatch) {
    return dispatch->read_fd;
}

unsigned int signal_dispatch_take(SignalDispatch *dispatch) {
    unsigned char buffer[64];
    unsigned int  pending = 0;
    sigset_t      old_mask;

    /* Blocking closes the race between draining the pipe and clearing flags. */
    if (sigprocmask(SIG_BLOCK, &dispatch->handled_signals, &old_mask) < 0)
        return 0;

    while (read(dispatch->read_fd, buffer, sizeof(buffer)) > 0)
        ;

    if (pending_term)
        pending |= SIGNAL_DISPATCH_TERM;
    if (pending_alrm)
        pending |= SIGNAL_DISPATCH_ALRM;
    if (pending_usr1)
        pending |= SIGNAL_DISPATCH_USR1;
    if (pending_usr2)
        pending |= SIGNAL_DISPATCH_USR2;
    pending_term = pending_alrm = pending_usr1 = pending_usr2 = 0;

    (void)sigprocmask(SIG_SETMASK, &old_mask, NULL);
    return pending;
}

unsigned int signal_dispatch_shutdown(SignalDispatch *dispatch) {
    struct itimerval timer = { 0 };
    sigset_t         old_mask;
    int              mask_blocked;
    unsigned int     pending = 0;

    mask_blocked = sigprocmask(SIG_BLOCK, &dispatch->handled_signals, &old_mask) == 0;
    (void)setitimer(ITIMER_REAL, &timer, NULL);

    if (pending_term)
        pending |= SIGNAL_DISPATCH_TERM;
    if (pending_alrm)
        pending |= SIGNAL_DISPATCH_ALRM;
    if (pending_usr1)
        pending |= SIGNAL_DISPATCH_USR1;
    if (pending_usr2)
        pending |= SIGNAL_DISPATCH_USR2;
    pending_term = pending_alrm = pending_usr1 = pending_usr2 = 0;

    install_noop_handler(SIGTERM);
    install_noop_handler(SIGALRM);
    if (dispatch->handle_usr1)
        install_noop_handler(SIGUSR1);
    if (dispatch->handle_usr2)
        install_noop_handler(SIGUSR2);
    signal_write_fd_code = 0;
    if (dispatch->read_fd >= 0)
        close(dispatch->read_fd);
    if (dispatch->write_fd >= 0)
        close(dispatch->write_fd);
    dispatch->read_fd = dispatch->write_fd = -1;
    if (mask_blocked)
        (void)sigprocmask(SIG_SETMASK, &old_mask, NULL);
    return pending;
}
