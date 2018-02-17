
#include <stdlib.h>
#include "log.h"
#include "memory.h"

int main(int argc, char* argv[])
{
    log_exit("test log. argc = %d", argc);
    int* p = xmalloc(sizeof(int));
    free(p);
    return 0;
}
