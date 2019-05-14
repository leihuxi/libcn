#include "event_timer.h"

rbtree_t event_timer_rbtree;
static rbtree_node_t event_timer_sentinel;

/*
 * the event timer rbtree may contain the duplicate keys, however,
 * it should not be a problem, because we use the rbtree to find
 * a minimum timer value only
 */

int32_t event_timer_init()
{
    rbtree_init(&event_timer_rbtree, &event_timer_sentinel,
                rbtree_insert_timer_value);

    return 0;
}


rbtree_key_t event_find_timer(void)
{
    rbtree_key_int_t timer;
    rbtree_node_t *node, *root, *sentinel;

    if (event_timer_rbtree.root == &event_timer_sentinel) {
        return TIMER_INFINITE;
    }

    root = event_timer_rbtree.root;
    sentinel = event_timer_rbtree.sentinel;

    node = rbtree_min(root, sentinel);

    timer = (rbtree_key_int_t)(node->key - current_msec);

    return (rbtree_key_t)(timer > 0 ? timer : 0);
}


void event_expire_timers(void)
{
    event_t *ev;
    rbtree_node_t *node, *root, *sentinel;

    sentinel = event_timer_rbtree.sentinel;

    for (;;) {
        root = event_timer_rbtree.root;

        if (root == sentinel) {
            return;
        }

        node = rbtree_min(root, sentinel);

        /* node->key > current_msec */

        if ((rbtree_key_int_t)(node->key - current_msec) > 0) {
            return;
        }

        ev = (event_t *)((char *)node - offsetof(event_t, timer));

        rbtree_delete(&event_timer_rbtree, &ev->timer);

        ev->timer_set = 0;
        ev->timedout = 1;
        ev->handler(ev);
    }
}


int32_t event_no_timers_left(void)
{
    event_t *ev;
    rbtree_node_t *node, *root, *sentinel;

    sentinel = event_timer_rbtree.sentinel;
    root = event_timer_rbtree.root;

    if (root == sentinel) {
        return 0;
    }

    for (node = rbtree_min(root, sentinel); node;
         node = rbtree_next(&event_timer_rbtree, node)) {
        ev = (event_t *)((char *)node - offsetof(event_t, timer));

        if (!ev->cancelable) {
            return -2;
        }
    }

    /* only cancelable timers left */
    return 0;
}

void event_del_timer(event_t *ev)
{
    rbtree_delete(&event_timer_rbtree, &ev->timer);

#if (DEBUG)
    ev->timer.left = NULL;
    ev->timer.right = NULL;
    ev->timer.parent = NULL;
#endif

    ev->timer_set = 0;
}

void event_add_timer(event_t *ev, rbtree_key_t timer)
{
    rbtree_key_t key;
    rbtree_key_int_t diff;

    key = current_msec + timer;

    if (ev->timer_set) {

        /*
         * Use a previous timer value if difference between it and a new
         * value is less than TIMER_LAZY_DELAY milliseconds: this allows
         * to minimize the rbtree operations for fast connections.
         */

        diff = (rbtree_key_int_t)(key - ev->timer.key);

        if (c_abs(diff) < TIMER_LAZY_DELAY) {
            return;
        }

        event_del_timer(ev);
    }

    ev->timer.key = key;

    rbtree_insert(&event_timer_rbtree, &ev->timer);
    ev->timer_set = 1;
}
#
