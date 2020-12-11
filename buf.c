#include "buf.h"

buf_t *create_temp_buf(pool_t *pool, size_t size)
{
    buf_t *b;

    b = calloc_buf(pool);
    if (b == NULL) {
        return NULL;
    }

    b->start = palloc(pool, size);
    if (b->start == NULL) {
        return NULL;
    }

    /*
     * set by calloc_buf():
     *
     *     b->file_pos = 0;
     *     b->file_last = 0;
     *     b->file = NULL;
     *     b->shadow = NULL;
     *     b->tag = 0;
     *     and flags
     */

    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;
    b->temporary = 1;

    return b;
}
