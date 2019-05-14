#ifndef _COMMON_INCLUDE_
#define _COMMON_INCLUDE_
#include <sys/types.h>
#include <unitypes.h>

typedef struct stime_s stime_t;
typedef struct shmtx_sh_s shmtx_sh_t;
typedef struct shmtx_s shmtx_t;
typedef struct queue_s queue_t;
typedef struct pool_data_s pool_data_t;
typedef struct pool_large_s pool_large_t;
typedef struct pool_s pool_t;
typedef struct list_s list_t;
typedef struct list_part_s list_part_t;
typedef struct array_s array_t;
typedef struct buf_s buf_t;

struct queue_s {
    queue_t *prev;
    queue_t *next;
};

typedef uint64_t rbtree_key_t;
typedef int64_t rbtree_key_int_t;
typedef struct rbtree_s rbtree_t;
typedef struct rbtree_node_s rbtree_node_t;
typedef void (*rbtree_insert_pt)(rbtree_node_t *root, rbtree_node_t *node,
                                 rbtree_node_t *sentinel);
struct rbtree_node_s {
    rbtree_key_t key;
    rbtree_node_t *left;
    rbtree_node_t *right;
    rbtree_node_t *parent;
    u_char color;
    u_char data;
};

struct rbtree_s {
    rbtree_node_t *root;
    rbtree_node_t *sentinel;
    rbtree_insert_pt insert;
};

struct list_part_s {
    void *elts;
    uint32_t nelts;
    list_part_t *next;
};

struct list_s {
    list_part_t *last;
    list_part_t part;
    size_t size;
    uint32_t nalloc;
    pool_t *pool;
};

struct array_s {
    void *elts;
    uint32_t nelts;
    size_t size;
    uint32_t nalloc;
    pool_t *pool;
};

typedef void *buf_tag_t;
struct buf_s {
    u_char *pos;
    u_char *last;
    off_t file_pos;
    off_t file_last;

    u_char *start; /* start of buffer */
    u_char *end;   /* end of buffer */
    buf_tag_t tag;
    buf_t *shadow;

    /* the buf's content could be changed */
    unsigned temporary : 1;

    /*
     * the buf's content is in a memory cache or in a read only memory
     * and must not be changed
     */
    unsigned memory : 1;

    /* the buf's content is mmap()ed and must not be changed */
    unsigned mmap : 1;

    unsigned recycled : 1;
    unsigned in_file : 1;
    unsigned flush : 1;
    unsigned sync : 1;
    unsigned last_buf : 1;
    unsigned last_in_chain : 1;

    unsigned last_shadow : 1;
    unsigned temp_file : 1;

    /* STUB */ int num;
};


#include "palloc.h"
#include "atomic.h"
#include "alloc.h"
#include "array.h"
#include "list.h"
#include "queue.h"
#include "shmtx.h"
#include "rbtree.h"
#include "times.h"
#include "buf.h"

#endif /* ifndef _COMMON_INCLUDE_*/
