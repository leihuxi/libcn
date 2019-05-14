#ifndef _EVENT_POSTED_H_INCLUDED_
#define _EVENT_POSTED_H_INCLUDED_
#include "core.h"

#define post_event(ev, q)                                                      \
    if (!(ev)->posted) {                                                       \
        (ev)->posted = 1;                                                      \
        queue_insert_tail(q, &(ev)->queue);                                    \
    } 

#define delete_posted_event(ev)                                                \
    (ev)->posted = 0;                                                          \
    queue_remove(&(ev)->queue);

void event_process_posted(cycle_t *cycle, queue_t *posted);

extern queue_t posted_accept_events;
extern queue_t posted_events;

#endif /* _EVENT_POSTED_H_INCLUDED_ */
