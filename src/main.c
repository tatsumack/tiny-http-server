#include <stdlib.h>
#include "sig.h"
#include "httpRequest.h"
#include "httpResponse.h"

static void service(FILE* in, FILE* out, char* docroot);

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

