#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>

int listen_socket(char* port);

typedef void (*AC_CALLBACK)(FILE*, FILE*, char*);
void accept_socket(int server_fd, char* docroot, AC_CALLBACK func);

#endif
