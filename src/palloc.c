#include "palloc.h"

static inline void *palloc_small(pool_t *pool, size_t size, uint32_t align);
static void *palloc_block(pool_t *pool, size_t size);
static void *palloc_large(pool_t *pool, size_t size);


pool_t *create_pool(size_t size)
{
    pool_t *p;

    p = memalign(POOL_ALIGNMENT, size);
    if (p == NULL) {
        return NULL;
    }

    p->d.last = (u_char *)p + sizeof(pool_t);
    p->d.end = (u_char *)p + size;
    p->d.next = NULL;
    p->d.failed = 0;

    size = size - sizeof(pool_t);
    p->max = (size < MAX_ALLOC_FROM_POOL) ? size : MAX_ALLOC_FROM_POOL;

    p->current = p;
    p->large = NULL;
    return p;
}


void destroy_pool(pool_t *pool)
{
    pool_t *p, *n;
    pool_large_t *l;
    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
        }
    }

    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
        free(p);

        if (n == NULL) {
            break;
        }
    }
}


void reset_pool(pool_t *pool)
{
    pool_t *p;
    pool_large_t *l;

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
        }
    }

    for (p = pool; p; p = p->d.next) {
        p->d.last = (u_char *)p + sizeof(pool_t);
        p->d.failed = 0;
    }

    pool->current = pool;
    pool->large = NULL;
}


void *palloc(pool_t *pool, size_t size)
{
    if (size <= pool->max) {
        return palloc_small(pool, size, 1);
    }
    return palloc_large(pool, size);
}


void *pnalloc(pool_t *pool, size_t size)
{
    if (size <= pool->max) {
        return palloc_small(pool, size, 0);
    }
    return palloc_large(pool, size);
}


static inline void *palloc_small(pool_t *pool, size_t size, uint32_t align)
{
    u_char *m;
    pool_t *p;

    p = pool->current;

    do {
        m = p->d.last;

        if (align) {
            m = align_ptr(m, ALIGNMENT);
        }

        if ((size_t)(p->d.end - m) >= size) {
            p->d.last = m + size;

            return m;
        }

        p = p->d.next;

    } while (p);

    return palloc_block(pool, size);
}


static void *palloc_block(pool_t *pool, size_t size)
{
    u_char *m;
    size_t psize;
    pool_t *p, *new;

    psize = (size_t)(pool->d.end - (u_char *)pool);

    m = memalign(POOL_ALIGNMENT, psize);
    if (m == NULL) {
        return NULL;
    }

    new = (pool_t *)m;

    new->d.end = m + psize;
    new->d.next = NULL;
    new->d.failed = 0;

    m += sizeof(pool_data_t);
    m = align_ptr(m, ALIGNMENT);
    new->d.last = m + size;

    for (p = pool->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            pool->current = p->d.next;
        }
    }

    p->d.next = new;

    return m;
}


static void *palloc_large(pool_t *pool, size_t size)
{
    void *p;
    uint32_t n;
    pool_large_t *large;

    p = alloc(size);
    if (p == NULL) {
        return NULL;
    }

    n = 0;

    for (large = pool->large; large; large = large->next) {
        if (large->alloc == NULL) {
            large->alloc = p;
            return p;
        }

        if (n++ > 3) {
            break;
        }
    }

    large = palloc_small(pool, sizeof(pool_large_t), 1);
    if (large == NULL) {
        free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


void *pmemalign(pool_t *pool, size_t size, size_t alignment)
{
    void *p;
    pool_large_t *large;

    p = memalign(alignment, size);
    if (p == NULL) {
        return NULL;
    }

    large = palloc_small(pool, sizeof(pool_large_t), 1);
    if (large == NULL) {
        free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


int32_t pfree(pool_t *pool, void *p)
{
    pool_large_t *l;

    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            free(l->alloc);
            l->alloc = NULL;
            return 0;
        }
    }

    return -1;
}


void *pcalloc(pool_t *pool, size_t size)
{
    void *p;

    p = palloc(pool, size);
    if (p) {
        memset(p, 0, size);
    }

    return p;
}
