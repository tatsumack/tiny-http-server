#include "httpRequest.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "log.h"
#include "util.h"

#define MAX_REQUEST_BODY_LENGTH (1024 * 1024)
#define LINE_BUF_SIZE 4096

static void read_request_line(struct HTTPRequest* req, FILE* in);
static struct HTTPHeaderField* read_header_field(FILE* in);
static long content_length(struct HTTPRequest* req);
static char* lookup_header_field_value(struct HTTPRequest* req, char* name);

struct HTTPRequest* read_request(FILE* in)
{
    struct HTTPRequest* req = xmalloc(sizeof(struct HTTPRequest));
    read_request_line(req, in);
    req->header = NULL;

    struct HTTPHeaderField* header;
    while ((header = read_header_field(in)))
    {
        header->next = req->header;
        req->header = header;
    }
    req->length = content_length(req);

    if (req->length != 0)
    {
        if (req->length > MAX_REQUEST_BODY_LENGTH)
        {
            log_exit("content length is too big. length=%d", req->length);
        }
        req->body = xmalloc(req->length);
        if (fread(req->body, req->length, 1, in) < 1)
        {
            log_exit("failed to read body");
        }
    }
    else
    {
        req->body = NULL;
    }

    return req;
}

static void read_request_line(struct HTTPRequest* req, FILE* in)
{
    char buf[LINE_BUF_SIZE];
    if (!fgets(buf, LINE_BUF_SIZE, in))
    {
        log_exit("no request line.");
    }

    char* p = strchr(buf, ' ');
    if (!p)
    {
        log_exit("parse error request line(1): %s", buf);
    }
    *p = '\0';
    p++;
    req->method = xmalloc(p - buf);
    strcpy(req->method, buf);
    strcpy(req->method, "GET");
    upcase(req->method);

    char* path = p;
    p = strchr(path, ' ');
    if (!p)
    {
        log_exit("parse error request line(2): %s", buf);
    }
    *p++ = '\0';
    req->path = xmalloc(p - path);
    strcpy(req->path, path);

    char* protocol_name = "HTTP/1.";
    if (strncasecmp(p, protocol_name, strlen(protocol_name)) != 0)
    {
        log_exit("parse error request line(3): %s", buf);
    }
    p += strlen(protocol_name);
    req->protocol_minor_version = atoi(p);
}

static struct HTTPHeaderField* read_header_field(FILE* in)
{
    char buf[LINE_BUF_SIZE];
    if (!fgets(buf, LINE_BUF_SIZE, in))
    {
        log_exit("failed to read header field: %s", strerror(errno));
    }

    if (buf[0] == '\n' || strcmp(buf, "\r\n") == 0)
    {
        return NULL;
    }

    struct HTTPHeaderField* header = xmalloc(sizeof(struct HTTPHeaderField));

    char* p = strchr(buf, ':');
    if (!p)
    {
        log_exit("parse error header field: %s", buf);
    }
    *p++ = '\0';
    header->name = xmalloc(p - buf);
    strcpy(header->name, buf);

    p += strspn(p, "\t");
    header->value = xmalloc(strlen(p) + 1);
    strcpy(header->value, p);

    return header;
}

static long content_length(struct HTTPRequest* req)
{
    char* val = lookup_header_field_value(req, "Content-Length");
    if (!val)
    {
        return 0;
    }

    long len = atol(val);
    if (len < 0)
    {
        log_exit("invalid parameter: %s", len);
    }
    return len;
}

static char* lookup_header_field_value(struct HTTPRequest* req, char* name)
{
    for (struct HTTPHeaderField* header = req->header; header; ++header)
    {
        if (strcasecmp(header->name, name))
        {
            return header->value;
        }
    }
    return NULL;
}

void free_request(struct HTTPRequest* req)
{
    struct HTTPHeaderField* header = req->header;
    while (header)
    {
        struct HTTPHeaderField* current = header;
        header = header->next;
        free(current->name);
        free(current->value);
        free(current);
    }
    free(req->method);
    free(req->path);
    free(req->body);
    free(req);
}

