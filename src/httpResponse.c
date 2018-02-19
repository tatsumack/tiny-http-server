#include "httpResponse.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "httpRequest.h"
#include "log.h"
#include "memory.h"

#define SERVER_VERSION "1.0"
#define HTTP_MINOR_VERSION 0

#define TIME_BUF_SIZE 64
#define BLOCK_BUF_SIZE 1024


static struct FileInfo* get_fileinfo(char* docroot, char* urlpath);
static char* build_fspath(char* docroot, char* urlpath);
static void do_file_response(struct HTTPRequest* req, FILE* out, char* docroot);
static void not_implemented(struct HTTPRequest* req, FILE* out);
static void output_common_header_fields(struct HTTPRequest* req, FILE* out, char* status);
static void not_found(struct HTTPRequest* req, FILE* out, char* path);
static void free_fileinfo(struct FileInfo* info);
static char* guess_content_type(struct FileInfo* info);

void respond_to(struct HTTPRequest* req, FILE* out, char* docroot)
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
        not_found(req, out, info->path);
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

static void not_found(struct HTTPRequest* req, FILE* out, char* path)
{
    output_common_header_fields(req, out, "404 Not Found");
    fprintf(out, "Content-Type: text/html\r\n");
    fprintf(out, "\r\n");
    if (strcmp(req->method, "HEAD") != 0) {
        fprintf(out, "<html>\r\n");
        fprintf(out, "<header><title>Not Found</title><header>\r\n");
        fprintf(out, "<body><p>File not found</p>%s</body>\r\n", path);
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

