#include "alloc.h"

void *alloc(size_t size)
{
    void *p;
    p = malloc(size);
    if (p == NULL) {
    }
    return p;
}


void *zalloc(size_t size)
{
    void *p;
    p = alloc(size);
    if (p) {
        memset(p, 0, size);
    }
    return p;
}


void *memalign(size_t alignment, size_t size)
{
    void *p;
    int err;

    err = posix_memalign(&p, alignment, size);

    if (err) {
        p = NULL;
    }
    return p;
}
