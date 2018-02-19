#include <stdlib.h>
#include <getopt.h>
#include <syslog.h>

#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include "log.h"

#include <grp.h>
#include <pwd.h>

#include "sig.h"
#include "httpRequest.h"
#include "httpResponse.h"

#define USAGE "Usage: %s [--port=n] [--chroot --user=u --group=g] <docroot>\n"

int debug_mode = 0;

static struct option longopts[] = {
    {"debug",  no_argument,       &debug_mode, 1},
    {"chroot", no_argument,       NULL,      'c'},
    {"user",   required_argument, NULL,      'u'},
    {"group",  required_argument, NULL,      'g'},
    {"port",   required_argument, NULL,      'p'},
    {"help",   no_argument,       NULL,      'h'},
    {0, 0, 0, 0}
};

static void service(FILE* in, FILE* out, char* docroot);
int listen_socket(char* port);
void server_main(int server_fd, char* docroot);

void become_daemon();

void setup_environment(char* root, char* user, char* group);

int main(int argc, char* argv[])
{
    int do_chroot = 0;
    char* port = NULL;
    char* user = NULL;
    char* group = NULL;
    int opt;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1)
    {
        switch (opt) {
            case 0:
                break;
            case 'c':
                do_chroot = 1;
                break;
            case 'u':
                user = optarg;
                break;
            case 'g':
                group = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'h':
                fprintf(stdout, USAGE, argv[0]);
                exit(0);
            case '?':
                fprintf(stdout, USAGE, argv[0]);
                exit(1);
        }
    }

    if (optind != argc - 1)
    {
        fprintf(stdout, USAGE, argv[0]);
        exit(1);
    }

    char* docroot = argv[optind];

    if (do_chroot)
    {
        setup_environment(docroot, user, group);
        docroot = "";
    }
    install_signal_handler();
    int server_fd = listen_socket(port);
    if (!debug_mode)
    {
        openlog(SERVER_NAME, LOG_PID|LOG_NDELAY, LOG_DAEMON);
        become_daemon();
    }
    server_main(server_fd, docroot);
    exit(0);
}

static void service(FILE* in, FILE* out, char* docroot)
{
    struct HTTPRequest* req = read_request(in);
    respond_to(req, out, docroot);
    free_request(req);
}

#define MAX_BACK_LOG 5
#define DEFAULT_PORT 80

int listen_socket(char* port)
{
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };

    struct addrinfo* res;
    int err = getaddrinfo(NULL, port, &hints, &res);
    if (err != 0)
    {
        log_exit(gai_strerror(err));
    }

    for (struct addrinfo* ai = res; ai; ai = ai->ai_next)
    {
        int sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0)
        {
            continue;
        }

        if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0)
        {
            close(sock);
            continue;
        }

        if (listen(sock, MAX_BACK_LOG) < 0)
        {
            close(sock);
            continue;
        }

        freeaddrinfo(res);
        return sock;
    }
    log_exit("failed to listen socket");
    return -1;

}

void server_main(int server_fd, char* docroot)
{
    for (;;)
    {
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof addr;
        int sock = accept(server_fd, (struct sockaddr*)& addr, &addrlen);
        if (sock < 0)
        {
            log_exit("accept(2) failed: %s", strerror(errno));
        }

        int pid = fork();
        if (pid < 0)
        {
            log_exit("fork(2) failed: %s", strerror(errno));
        }
        if (pid == 0)
        {
            FILE* in = fdopen(sock, "r");
            FILE* out = fdopen(sock, "w");
            service(in, out, docroot);
            exit(0);
        }
        close(sock);
    }
}

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
