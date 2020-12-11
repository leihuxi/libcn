#ifndef _PALLOC_H_INCLUDED_
#define _PALLOC_H_INCLUDED_
#include "core.h"

struct pool_large_s {
    pool_large_t *next;
    void *alloc;
};

struct pool_data_s {
    u_char *last;
    u_char *end;
    pool_t *next;
    uint32_t failed;
};

struct pool_s {
    pool_data_t d;
    size_t max;
    pool_t *current;
    pool_large_t *large;
};

/*
 * MAX_ALLOC_FROM_POOL should be (pagesize - 1), i.e. 4095 on x86.
 * On Windows NT it decreases a number of locked pages in a kernel.
 */
#define MAX_ALLOC_FROM_POOL (4096 - 1)

#define DEFAULT_POOL_SIZE (16 * 1024)
#define ALIGNMENT sizeof(unsigned long)

#define POOL_ALIGNMENT 16
#define align(d, a) (((d) + (a - 1)) & ~(a - 1))
#define align_ptr(p, a)                                                        \
    (u_char *)(((uintptr_t)(p) + ((uintptr_t)a - 1)) & ~((uintptr_t)a - 1))


#define MIN_POOL_SIZE                                                          \
    align((sizeof(pool_t) + 2 * sizeof(pool_large_t)), POOL_ALIGNMENT)

pool_t *create_pool(size_t size);
void destroy_pool(pool_t *pool);
void reset_pool(pool_t *pool);

void *palloc(pool_t *pool, size_t size);
void *pnalloc(pool_t *pool, size_t size);
void *pcalloc(pool_t *pool, size_t size);
void *pmemalign(pool_t *pool, size_t size, size_t alignment);
int32_t pfree(pool_t *pool, void *p);

#endif /* _PALLOC_H_INCLUDED_ */
