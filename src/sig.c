#include "sig.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include "log.h"

typedef void (*sighandler_t)(int);
static void trap_signal(int sig, sighandler_t handler);
static void signal_exit(int sig);
static void detach_children();
static void wait_handler(int sig);

void install_signal_handler()
{
    trap_signal(SIGPIPE, signal_exit);
    detach_children();
}

static void trap_signal(int sig, sighandler_t handler)
{
    struct sigaction act = {
        .sa_handler = handler,
        .sa_flags = SA_RESTART
    };
    sigemptyset(&act.sa_mask);
    if (sigaction(sig, &act, NULL) < 0)
    {
        log_exit("sigaction() failed. : %s", strerror(errno));
    }

}

static void detach_children()
{
    struct sigaction act = {
        .sa_handler = wait_handler,
        .sa_flags = SA_RESTART
    };
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGCHLD, &act, NULL) < 0)
    {
        log_exit("sigaction() failed. : %s", strerror(errno));
    }
}

static void wait_handler(int sig)
{
    wait(NULL);
}

static void signal_exit(int sig)
{
    log_exit("exit by signal. signal=%d", sig);
}
