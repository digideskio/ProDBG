#include "alloc.h"
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void* alloc_zero(int size)
{
    void* t = malloc((size_t)size);
    memset(t, 0, (size_t)size);
    return t;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
