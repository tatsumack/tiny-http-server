#include "socket.h"

#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"

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

void accept_socket(int server_fd, char* docroot, AC_CALLBACK callback)
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
            callback(in, out, docroot);
            exit(0);
        }
        close(sock);
    }
}

