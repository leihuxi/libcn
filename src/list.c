#include "list.h"
#include "palloc.h"

list_t *list_create(pool_t *pool, uint32_t n, size_t size)
{
    list_t *list;

    list = palloc(pool, sizeof(list_t));
    if (list == NULL) {
        return NULL;
    }

    if (list_init(list, pool, n, size) != 0) {
        return NULL;
    }

    return list;
}


void *list_push(list_t *l)
{
    void *elt;
    list_part_t *last;

    last = l->last;

    if (last->nelts == l->nalloc) {

        /* the last part is full, allocate a new list part */

        last = palloc(l->pool, sizeof(list_part_t));
        if (last == NULL) {
            return NULL;
        }

        last->elts = palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL) {
            return NULL;
        }

        last->nelts = 0;
        last->next = NULL;

        l->last->next = last;
        l->last = last;
    }

    elt = (char *)last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}

int32_t list_init(list_t *list, pool_t *pool, uint32_t n, size_t size)
{
    list->part.elts = palloc(pool, n * size);
    if (list->part.elts == NULL) {
        return -1;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;
    return 0;
}
