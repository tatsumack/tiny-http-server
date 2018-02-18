#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <stdio.h>

struct HTTPHeaderField {
    char* name;
    char* value;
    struct HTTPHeaderField* next;
};

struct HTTPRequest {
    int protocol_minor_version;
    char* method;
    char* path;
    struct HTTPHeaderField* header;
    char* body;
    long length;
};

struct HTTPRequest* read_request(FILE* in);
void free_request(struct HTTPRequest* req);

#endif
