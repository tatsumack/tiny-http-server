
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void log_exit(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);

    exit(1);
}

