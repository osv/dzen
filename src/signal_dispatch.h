#ifndef SIGNAL_DISPATCH_H
#define SIGNAL_DISPATCH_H

#include <signal.h>

/* Signals reported by signal_dispatch_take(). */
enum {
    SIGNAL_DISPATCH_TERM = 1 << 0,
    SIGNAL_DISPATCH_ALRM = 1 << 1,
    SIGNAL_DISPATCH_USR1 = 1 << 2,
    SIGNAL_DISPATCH_USR2 = 1 << 3
};

typedef struct {
    int      read_fd;
    int      write_fd;
    sigset_t handled_signals;
    int      handle_usr1;
    int      handle_usr2;
} SignalDispatch;

/*
 * Install handlers and create a nonblocking, close-on-exec self-pipe.
 * SIGTERM and SIGALRM are always handled; SIGUSR1/2 are optional. Handled
 * signals are unblocked after their handlers have been installed.
 */
int          signal_dispatch_init(SignalDispatch *dispatch, int handle_usr1, int handle_usr2);
/* Return the descriptor that must be monitored for readability. */
int          signal_dispatch_fd(const SignalDispatch *dispatch);
/* Drain the self-pipe and atomically take the coalesced pending signals. */
unsigned int signal_dispatch_take(SignalDispatch *dispatch);
/* Atomically take pending signals, disable dispatch, and close the self-pipe. */
unsigned int signal_dispatch_shutdown(SignalDispatch *dispatch);

#endif
