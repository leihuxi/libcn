#include "epoll_event.h"

static void epoll_test_rdhup()
{
    int s[2], events;
    struct epoll_event ee;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, s) == -1) {
        return;
    }
    ee.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
    if (epoll_ctl(ep, EPOLL_CTL_ADD, s[0], &ee) == -1) {
        goto failed;
    }
    if (close(s[1]) == -1) {
        s[1] = -1;
        goto failed;
    }

    s[1] = -1;

    events = epoll_wait(ep, &ee, 1, 5000);
    if (events == -1) {
        goto failed;
    }
    if (events) {
        use_epoll_rdhup = ee.events & EPOLLRDHUP;
    }
failed:
    if (s[1] != -1 && close(s[1]) == -1) {
    }
    if (close(s[0]) == -1) {
    }
}

int32_t epoll_init(cycle_t *cycle)
{
    if (ep == -1) {
        ep = epoll_create(cycle->connection_n / 2);
        if (ep == -1) {
            return -1;
        }
        epoll_test_rdhup();
    }

    if (nevents < EPOLL_EVETNS) {
        if (event_list) {
            free(event_list);
        }

        event_list = alloc(sizeof(struct epoll_event) * EPOLL_EVETNS);
        if (event_list == NULL) {
            return -1;
        }
    }

    nevents = EPOLL_EVETNS;
    return 0;
}

void epoll_done(cycle_t *cycle)
{
    if (close(ep) == -1) {
    }

    ep = -1;
    free(event_list);
    event_list = NULL;
    nevents = 0;
}

int32_t epoll_add_event(event_t *ev, int32_t event, uint32_t flags)
{
    int op;
    uint32_t events, prev;
    event_t *e;
    connection_t *c;
    struct epoll_event ee;

    c = ev->data;

    events = (uint32_t)event;

    if (event == READ_EVENT) {
        e = c->write;
        prev = EPOLLOUT;
#if (READ_EVENT != EPOLLIN | EPOLLRDHUP)
        events = EPOLLIN | EPOLLRDHUP;
#endif

    } else {
        e = c->read;
        prev = EPOLLIN | EPOLLRDHUP;
#if (WRITE_EVENT != EPOLLOUT)
        events = EPOLLOUT;
#endif
    }

    if (e->active) {
        op = EPOLL_CTL_MOD;
        events |= prev;

    } else {
        op = EPOLL_CTL_ADD;
    }

#if (HAVE_EPOLLEXCLUSIVE && HAVE_EPOLLRDHUP)
    if (flags & EXCLUSIVE_EVENT) {
        events &= ~EPOLLRDHUP;
    }
#endif

    ee.events = events | (uint32_t)flags;
    ee.data.ptr = (void *)((uintptr_t)c | ev->instance);

    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        return -1;
    }

    ev->active = 1;
    return 0;
}


int32_t epoll_del_event(event_t *ev, int32_t event, uint32_t flags)
{
    int op;
    uint32_t prev;
    event_t *e;
    connection_t *c;
    struct epoll_event ee;

    /*
     * when the file descriptor is closed, the epoll automatically deletes
     * it from its queue, so we do not need to delete explicitly the event
     * before the closing the file descriptor
     */
    if (flags & CLOSE_EVENT) {
        ev->active = 0;
        return 0;
    }

    c = ev->data;

    if (event == READ_EVENT) {
        e = c->write;
        prev = EPOLLOUT;

    } else {
        e = c->read;
        prev = EPOLLIN | EPOLLRDHUP;
    }

    if (e->active) {
        op = EPOLL_CTL_MOD;
        ee.events = prev | (uint32_t)flags;
        ee.data.ptr = (void *)((uintptr_t)c | ev->instance);

    } else {
        op = EPOLL_CTL_DEL;
        ee.events = 0;
        ee.data.ptr = NULL;
    }

    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        return -1;
    }

    ev->active = 0;

    return 0;
}


int32_t epoll_add_connection(connection_t *c)
{
    struct epoll_event ee;

    ee.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
    ee.data.ptr = (void *)((uintptr_t)c | c->read->instance);
    if (epoll_ctl(ep, EPOLL_CTL_ADD, c->fd, &ee) == -1) {
        return 0;
    }

    c->read->active = 1;
    c->write->active = 1;

    return 0;
}


int32_t epoll_del_connection(connection_t *c, uint32_t flags)
{
    int op;
    struct epoll_event ee;

    /*
     * when the file descriptor is closed the epoll automatically deletes
     * it from its queue so we do not need to delete explicitly the event
     * before the closing the file descriptor
     */
    if (flags & CLOSE_EVENT) {
        c->read->active = 0;
        c->write->active = 0;
        return 0;
    }

    op = EPOLL_CTL_DEL;
    ee.events = 0;
    ee.data.ptr = NULL;

    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        return -1;
    }

    c->read->active = 0;
    c->write->active = 0;

    return 0;
}


int32_t epoll_process_events(cycle_t *cycle, rbtree_key_t timer, uint32_t flags)
{
    int events;
    uint32_t revents;
    int32_t instance, i;
    uint32_t level;
    event_t *rev, *wev;
    queue_t *queue;
    connection_t *c;

    /* TIMER_INFINITE == INFTIM */

    events = epoll_wait(ep, event_list, (int)nevents, timer);

    if (flags & UPDATE_TIME || event_timer_alarm) {
        stime_update();
    }

    if (events == -1) {
        if (errno == EINTR) {
            if (event_timer_alarm) {
                event_timer_alarm = 0;
                return 0;
            }
        }
        return -1;
    }

    if (events == 0) {
        if (timer != TIMER_INFINITE) {
            return 0;
        }
        return -1;
    }

    for (i = 0; i < events; i++) {
        c = event_list[i].data.ptr;

        instance = (uintptr_t)c & 1;
        c = (connection_t *)((uintptr_t)c & (uintptr_t)~1);

        rev = c->read;

        if (c->fd == -1 || rev->instance != instance) {

            /*
             * the stale event from a file descriptor
             * that was just closed in this iteration
             */
            continue;
        }

        revents = event_list[i].events;

        if (revents & (EPOLLERR | EPOLLHUP)) {
            /*
             * if the error events were returned, add EPOLLIN and EPOLLOUT
             * to handle the events at least in one active handler
             */

            revents |= EPOLLIN | EPOLLOUT;
        }

        if ((revents & EPOLLIN) && rev->active) {

            rev->ready = 1;
            if (revents & EPOLLRDHUP) {
                rev->pending_eof = 1;
            }
            rev->available = 1;

            if (flags & POST_EVENTS) {
                queue = rev->accept ? &posted_accept_events : &posted_events;

                post_event(rev, queue);

            } else {
                rev->handler(rev);
            }
        }

        wev = c->write;

        if ((revents & EPOLLOUT) && wev->active) {

            if (c->fd == -1 || wev->instance != instance) {

                /*
                 * the stale event from a file descriptor
                 * that was just closed in this iteration
                 */

                continue;
            }

            wev->ready = 1;
            if (flags & POST_EVENTS) {
                post_event(wev, &posted_events);
            } else {
                wev->handler(wev);
            }
        }
    }

    return 0;
}
