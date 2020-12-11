#ifndef _BUF_H_INCLUDED_
#define _BUF_H_INCLUDED_
#include "core.h"

#define buf_in_memory(b) (b->temporary || b->memory || b->mmap)
#define buf_in_memory_only(b) (buf_in_memory(b) && !b->in_file)

#define buf_special(b)                                                         \
    ((b->flush || b->last_buf || b->sync) && !buf_in_memory(b) && !b->in_file)

#define buf_sync_only(b)                                                       \
    (b->sync && !buf_in_memory(b) && !b->in_file && !b->flush && !b->last_buf)

#define buf_size(b)                                                            \
    (buf_in_memory(b) ? (off_t)(b->last - b->pos)                              \
                      : (b->file_last - b->file_pos))

buf_t *create_temp_buf(pool_t *pool, size_t size);

#define alloc_buf(pool) palloc(pool, sizeof(buf_t))
#define calloc_buf(pool) pcalloc(pool, sizeof(buf_t))

#endif /* _BUF_H_INCLUDED_ */
