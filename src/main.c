#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "log.h"
#include "memory.h"
#include "sig.h"

#define SERVER_NAME "tinyHTTPServer"
#define SERVER_VERSION "1.0"
#define HTTP_MINOR_VERSION 0

#define LINE_BUF_SIZE 4096
#define MAX_REQUEST_BODY_LENGTH (1024 * 1024)

#define BLOCK_BUF_SIZE 1024
#define TIME_BUF_SIZE 64

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

struct FileInfo {
    char* path;
    long size;
    bool ok;
};

static void service(FILE* in, FILE* out, char* docroot);

static struct HTTPRequest* read_request(FILE* in);
static void read_request_line(struct HTTPRequest* req, FILE* in);
static struct HTTPHeaderField* read_header_field(FILE* in);
static long content_length(struct HTTPRequest* req);
static char* lookup_header_field_value(struct HTTPRequest* req, char* name);
static void respond_to(struct HTTPRequest* req, FILE* out, char* docroot);
static void free_request(struct HTTPRequest* req);

static struct FileInfo* get_fileinfo(char* docroot, char* urlpath);
static char* build_fspath(char* docroot, char* urlpath);
static void do_file_response(struct HTTPRequest* req, FILE* out, char* docroot);
static void not_implemented(struct HTTPRequest* req, FILE* out);
static void output_common_header_fields(struct HTTPRequest* req, FILE* out, char* status);
static void not_found(struct HTTPRequest* req, FILE* out);
static void free_fileinfo(struct FileInfo* info);
static char* guess_content_type(struct FileInfo* info);

static void upcase(char *str);


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <docroot> \n", argv[0]);
        exit(1);
    }
    install_signal_handler();
    service(stdin, stdout, argv[1]);
    return 0;
}

static void service(FILE* in, FILE* out, char* docroot)
{
    struct HTTPRequest* req = read_request(in);
    respond_to(req, out, docroot);
    free_request(req);
}

static struct HTTPRequest* read_request(FILE* in)
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

static void respond_to(struct HTTPRequest* req, FILE* out, char* docroot)
{
    if (strcmp(req->method, "GET") == 0)
    {
        do_file_response(req, out, docroot);
    }
    else if (strcmp(req->method, "HEAD") == 0)
    {
        do_file_response(req, out, docroot);
    }
    else
    {
        not_implemented(req, out);
    }
}
static void do_file_response(struct HTTPRequest* req, FILE* out, char* docroot)
{
    struct FileInfo* info = get_fileinfo(docroot, req->path);
    if (!info || !info->ok)
    {
        printf("path:%s\n", req->path);
        free_fileinfo(info);
        not_found(req, out);
        return;
    }

    output_common_header_fields(req, out, "200 OK");

    fprintf(out, "Content-Length: %ld\r\n", info->size);
    fprintf(out, "Content-Type: %s\r\n", guess_content_type(info));
    fprintf(out, "\r\n");

    if (strcmp(req->method, "HEAD") != 0)
    {
        int fd = open(info->path, O_RDONLY);
        if (fd < 0)
        {
            log_exit("failed to open %s: %s", info->path, strerror(errno));
        }
        for (;;)
        {
            char buf[BLOCK_BUF_SIZE];
            int n = read(fd, buf, BLOCK_BUF_SIZE);
            if (n < 0)
            {
                log_exit("failed to read %s: %s", info->path, strerror(errno));
            }
            if (n == 0)
            {
                break;
            }
            if (fwrite(buf, 1, n, out) < n)
            {
                log_exit("failed to write : %s", strerror(errno));
            }
        }
        close(fd);
    }
    fflush(out);
    free_fileinfo(info);
}

static char* guess_content_type(struct FileInfo* info)
{
    return "text/plain";
}


static void not_implemented(struct HTTPRequest* req, FILE* out)
{
    output_common_header_fields(req, out, "501 Not Implemented");
    fprintf(out, "Content-Type: text/html\r\n");
    fprintf(out, "\r\n");
    fprintf(out, "<html>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<title>501 Not Implemented</title>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<body>\r\n");
    fprintf(out, "<p>The request method %s is not implemented</p>\r\n", req->method);
    fprintf(out, "</body>\r\n");
    fprintf(out, "</html>\r\n");
    fflush(out);
}

static void not_found(struct HTTPRequest* req, FILE* out)
{
    output_common_header_fields(req, out, "404 Not Found");
    fprintf(out, "Content-Type: text/html\r\n");
    fprintf(out, "\r\n");
    if (strcmp(req->method, "HEAD") != 0) {
        fprintf(out, "<html>\r\n");
        fprintf(out, "<header><title>Not Found</title><header>\r\n");
        fprintf(out, "<body><p>File not found</p></body>\r\n");
        fprintf(out, "</html>\r\n");
    }
    fflush(out);
}

static void output_common_header_fields(struct HTTPRequest* req, FILE* out, char* status)
{
    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    if (!tm)
    {
        log_exit("gmtime() failed: %s", strerror(errno));
    }

    char buf[TIME_BUF_SIZE];
    strftime(buf, TIME_BUF_SIZE, "%a, %d %b %Y %H:%M:%S GMT", tm);

    fprintf(out, "HTTP/1.%d %s\r\n", HTTP_MINOR_VERSION, status);
    fprintf(out, "Date: %s\r\n", buf);
    fprintf(out, "Server: %s/%s\r\n", SERVER_NAME, SERVER_VERSION);
    fprintf(out, "Connection: close\r\n");
}

static void free_request(struct HTTPRequest* req)
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

static void upcase(char *str)
{
    for (char* p = str; *p; p++)
    {
        *p = (char)toupper((int)*p);
    }
}

static struct FileInfo* get_fileinfo(char* docroot, char* urlpath)
{
    struct FileInfo* info = xmalloc(sizeof(struct FileInfo));
    info->path = build_fspath(docroot, urlpath);
    info->ok = false;

    struct stat st;
    if (lstat(info->path, &st) < 0)
    {
        return info;
    }
    if (!S_ISREG(st.st_mode))
    {
        return info;
    }
    info->ok = true;
    info->size = st.st_size;

    return info;
}
static void free_fileinfo(struct FileInfo* info)
{
    if (!info)
    {
        return;
    }
    free(info->path);
    free(info);
}

static char* build_fspath(char* docroot, char* urlpath)
{
    char* path = xmalloc(strlen(docroot) + 1 + strlen(urlpath) + 1);
    sprintf(path, "%s/%s", docroot, urlpath);
    return path;
}
