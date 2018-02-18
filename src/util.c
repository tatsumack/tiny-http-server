#include "util.h"

#include <ctype.h>

void upcase(char *str)
{
    for (char* p = str; *p; p++)
    {
        *p = (char)toupper((int)*p);
    }
}

