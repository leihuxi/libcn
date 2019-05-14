#ifndef _EVENT_TIMER_H_INCLUDED_
#define _EVENT_TIMER_H_INCLUDED_
#include "rbtree.h"
#include "core.h"

#define TIMER_INTERVAL 1000
#define TIMER_INFINITE (rbtree_key_t) - 1
#define TIMER_LAZY_DELAY 300

int32_t event_timer_init();
rbtree_key_t event_find_timer(void);
void event_expire_timers(void);
int32_t event_no_timers_left(void);
void event_del_timer(event_t *ev);
void event_add_timer(event_t *ev, rbtree_key_t timer);

extern rbtree_t event_timer_rbtree;

#endif /* _EVENT_TIMER_H_INCLUDED_ */
