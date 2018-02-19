#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <stdio.h>
#include <stdbool.h>
#include "httpRequest.h"

#define SERVER_NAME "TinyHTTPServer"

struct FileInfo {
    char* path;
    long size;
    bool ok;
};

void respond_to(struct HTTPRequest* req, FILE* out, char* docroot);

#endif
