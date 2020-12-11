#ifndef _EVENT_H_INCLUDED_
#define _EVENT_H_INCLUDED_
#include "core.h"
#include "queue.h"
#include "rbtree.h"

typedef void (*event_handler_pt)(event_t *ev);

struct event_s {
    void *data;

    unsigned write : 1;

    unsigned accept : 1;

    /* used to detect the stale events in kqueue and epoll */
    unsigned instance : 1;

    /*
     * the event was passed or would be passed to a kernel;
     * in aio mode - operation was posted.
     */
    unsigned active : 1;

    unsigned disabled : 1;

    /* the ready event; in aio mode 0 means that no operation can be posted */
    unsigned ready : 1;

    unsigned oneshot : 1;

    /* aio operation is complete */
    unsigned complete : 1;

    unsigned eof : 1;
    unsigned error : 1;

    unsigned timedout : 1;
    unsigned timer_set : 1;

    unsigned delayed : 1;

    unsigned deferred_accept : 1;

    /* the pending eof reported by kqueue, epoll or in aio chain operation */
    unsigned pending_eof : 1;

    unsigned posted : 1;

    unsigned closed : 1;

    /* to test on worker exit */
    unsigned channel : 1;
    unsigned resolver : 1;

    unsigned cancelable : 1;

    /*
     * kqueue only:
     *   accept:     number of sockets that wait to be accepted
     *   read:       bytes to read when event is ready
     *               or lowat when event is set with LOWAT_EVENT flag
     *   write:      available space in buffer when event is ready
     *               or lowat when event is set with LOWAT_EVENT flag
     *
     * epoll with EPOLLRDHUP:
     *   accept:     1 if accept many, 0 otherwise
     *   read:       1 if there can be data to read, 0 otherwise
     *
     * iocp: TODO
     *
     * otherwise:
     *   accept:     1 if accept many, 0 otherwise
     */

    unsigned available : 1;

    event_handler_pt handler;

    uint64_t index;

    rbtree_node_t timer;

    /* the posted queue */
    queue_t queue;
};

#define UPDATE_TIME 1
#define POST_EVENTS 2

#define CLOSE_EVENT 1

#define ACCEPT_MUTEX_DELAY 500

extern atomic_t *stat_accepted;
extern atomic_t *stat_handled;
extern atomic_t *stat_requests;
extern atomic_t *stat_active;
extern atomic_t *stat_reading;
extern atomic_t *stat_writing;
extern atomic_t *stat_waiting;

extern atomic_t *connection_counter;
extern atomic_t *accept_mutex_ptr;

extern shmtx_t accept_mutex;
extern uint32_t use_accept_mutex;
extern uint32_t accept_events;
extern uint32_t accept_mutex_held;
extern rbtree_key_t accept_mutex_delay;
extern int32_t accept_disabled;

extern sig_atomic_t event_timer_alarm;
extern uint32_t event_flags;

void udp_rbtree_insert_value(rbtree_node_t *temp,
                             rbtree_node_t *node,
                             rbtree_node_t *sentinel);

void delete_udp_connection(void *data);
int32_t trylock_accept_mutex(cycle_t *cycle);
int32_t enable_accept_events(cycle_t *cycle);
void process_events_and_timers(cycle_t *cycle);
int32_t event_process_init(cycle_t *cycle);
int32_t event_master_init(cycle_t *cycle);
int32_t handle_read_event(event_t *rev);
int32_t handle_write_event(event_t *wev, size_t lowat);

void event_accept(event_t *ev);
void event_recvmsg(event_t *ev);
#endif /* _EVENT_H_INCLUDED_ */
