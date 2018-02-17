#include "memory.h"

#include <stdlib.h>
#include "log.h"

void* xmalloc(size_t size)
{
    void* p = malloc(size);
    if (!p) log_exit("cannot allocate memory. size=%d", size);
    return p;
}

