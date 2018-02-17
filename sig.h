#ifndef SIG_H
#define SIG_H

void install_singal_handlers();

typedef void (*sighandler_t)(int);
static void trap_signal(int sig, sighandler_t handler);

static void signal_exit(int sig);

#endif

