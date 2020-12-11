#ifndef _CYCLE_H_INCLUDED_
#define _CYCLE_H_INCLUDED_
#include "core.h"
#define MAX_CONN_N 512;

struct cycle_s {
    pool_t *pool;

    connection_t **files;
    uint32_t files_n;
    connection_t *free_connections;
    uint32_t free_connection_n;

    queue_t reusable_connections_queue;
    uint32_t reusable_connections_n;

    listening_t *listening;

    uint32_t connection_n;
    connection_t *connections;

    event_t *read_events;
    event_t *write_events;
};

extern cycle_t *cycle;
cycle_t *init_cycle();
#endif /* _CYCLE_H_INCLUDED_ */
