#include "process.h"

#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "log.h"

void become_daemon()
{
    if (chdir("/") < 0)
    {
        log_exit("chdir(2) failed: %s", strerror(errno));
    }
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    int n = fork();
    if (n < 0)
    {
        log_exit("fork(2) failed: %s", strerror(errno));
    }
    if (n != 0)
    {
        _exit(0);
    }
    if (setsid() < 0)
    {
        log_exit("setsid(2) failed: %s", strerror(errno));
    }
}

void setup_environment(char* root, char* user, char* group)
{
    if (!user || !group)
    {
        fprintf(stderr, "use both of --user and --group\n");
        exit(1);
    }

    struct group *gr = getgrnam(group);
    if (!gr)
    {
        fprintf(stderr, "no such group: %s\n", group);
        exit(1);
    }

    if (setgid(gr->gr_gid) < 0)
    {
        perror("setgid(2)");
        exit(1);
    }
    if (initgroups(user, gr->gr_gid) < 0)
    {
        perror("initgroups(2)");
        exit(1);
    }

    struct passwd* pw = getpwnam(user);
    if (!pw)
    {
        fprintf(stderr, "no such user: %s\n", user);
        exit(1);
    }

    chroot(root);
    if(setuid(pw->pw_uid) < 0)
    {
        perror("setuid(2)");
        exit(1);
    }
}

