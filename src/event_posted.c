#include "event_posted.h"

queue_t posted_accept_events;
queue_t posted_events;
void event_process_posted(cycle_t *cycle, queue_t *posted)
{
    queue_t *q;
    event_t *ev;

    while (!queue_empty(posted)) {

        q = queue_head(posted);
        ev = queue_data(q, event_t, queue);

        delete_posted_event(ev);

        ev->handler(ev);
    }
}
