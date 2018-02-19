#include <stdlib.h>
#include <getopt.h>
#include <syslog.h>

#include "sig.h"
#include "httpRequest.h"
#include "httpResponse.h"
#include "socket.h"
#include "process.h"

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
    accept_socket(server_fd, docroot, service);
    exit(0);
}

static void service(FILE* in, FILE* out, char* docroot)
{
    struct HTTPRequest* req = read_request(in);
    respond_to(req, out, docroot);
    free_request(req);
}

