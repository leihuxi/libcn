#include "core.h"

static int32_t disable_accept_events(cycle_t *cycle, uint32_t all);
static void close_accepted_connection(connection_t *c);

// TODO:
void event_accept(event_t *ev)
{
    socklen_t socklen;
    uint32_t level;
    int s, flags;
    event_t *rev, *wev;
    struct sockaddr sa;
    listening_t *ls;
    connection_t *c, *lc;

    if (ev->timedout) {
        if (enable_accept_events((cycle_t *)cycle) != 0) {
            return;
        }

        ev->timedout = 0;
    }

    lc = ev->data;
    ls = lc->listening;
    ev->ready = 0;
    ev->available = 1;

    do {
        socklen = sizeof(struct sockaddr);
        s = accept(lc->fd, &sa, &socklen);

        if (s == (int)-1) {
            if (errno == EAGAIN) {
                return;
            }

            if (errno == ECONNABORTED) {
                if (ev->available) {
                    continue;
                }
            }

            if (errno == EMFILE || errno == ENFILE) {
                if (disable_accept_events((cycle_t *)cycle, 1) != 0) {
                    return;
                }

                if (use_accept_mutex) {
                    if (accept_mutex_held) {
                        shmtx_unlock(&accept_mutex);
                        accept_mutex_held = 0;
                    }
                    accept_disabled = 1;
                } else {
                    event_add_timer(ev, ACCEPT_MUTEX_DELAY);
                }
            }
            return;
        }

        accept_disabled = cycle->connection_n / 8 - cycle->free_connection_n;

        (void)atomic_fetch_add(stat_accepted, 1);
        c = get_connection(s, cycle);
        if (c == NULL) {
            if (close(s) == -1) {
            }
            return;
        }

        c->type = SOCK_STREAM;
        (void)atomic_fetch_add(stat_active, 1);

        c->pool = create_pool(ls->pool_size);
        if (c->pool == NULL) {
            close_accepted_connection(c);
            return;
        }

        if (socklen > (int)sizeof(struct sockaddr)) {
            socklen = sizeof(struct sockaddr);
        }

        c->sockaddr = palloc(c->pool, socklen);
        if (c->sockaddr == NULL) {
            close_accepted_connection(c);
            return;
        }

        memcpy(c->sockaddr, &sa, socklen);

        /* set a blocking mode for iocp and non-blocking mode for others */
        flags = fcntl(s, F_GETFL, 0);
        if (flags == -1) {
            flags = 0;
        }
        fcntl(s, F_SETFL, flags | O_NONBLOCK);


        c->recv = posix_recv;
        c->send = posix_send;
        /* c->recv_chain = recv_chain; */
        /* c->send_chain = send_chain; */

        c->socklen = socklen;
        c->listening = ls;
        c->local_sockaddr = ls->sockaddr;
        c->local_socklen = ls->socklen;
        rev = c->read;
        wev = c->write;
        wev->ready = 1;
        if (ev->deferred_accept) {
            rev->ready = 1;
            rev->available = 1;
        }

        c->number = atomic_fetch_add(connection_counter, 1);
        (void)atomic_fetch_add(stat_handled, 1);

        ls->handler(c);
    } while (ev->available);
}

int32_t trylock_accept_mutex(cycle_t *cycle)
{
    if (shmtx_trylock(&accept_mutex)) {
        if (accept_mutex_held && accept_events == 0) {
            return 0;
        }

        if (enable_accept_events(cycle) == -1) {
            shmtx_unlock(&accept_mutex);
            return -1;
        }

        accept_events = 0;
        accept_mutex_held = 1;
        return 0;
    }

    if (accept_mutex_held) {
        if (disable_accept_events(cycle, 0) == -1) {
            return -1;
        }

        accept_mutex_held = 0;
    }
    return 0;
}


int32_t enable_accept_events(cycle_t *cycle)
{
    uint32_t i;
    listening_t *ls;
    connection_t *c;

    ls = cycle->listening;
    c = ls->connection;

    if (c == NULL || c->read->active) {
        return 0;
    }

    if (epoll_add_event(c->read, READ_EVENT, 0) == -1) {
        return -1;
    }
    return 0;
}


static int32_t disable_accept_events(cycle_t *cycle, uint32_t all)
{
    uint32_t i;
    listening_t *ls;
    connection_t *c;

    ls = cycle->listening;
    c = ls->connection;

    if (c == NULL || !c->read->active) {
        return 0;
    }

    /*
     * do not disable accept on worker's own sockets
     * when disabling accept events due to accept mutex
     */

    if (ls->reuseport && !all) {
        return 0;
    }

    if (epoll_del_event(c->read, READ_EVENT, 0) == -1) {
        return -1;
    }
    return 0;
}

static void close_accepted_connection(connection_t *c)
{
    int fd;

    free_connection(c);

    fd = c->fd;
    c->fd = (int)-1;

    if (close(fd) == -1) {
    }

    if (c->pool) {
        destroy_pool(c->pool);
    }
    (void)atomic_fetch_add(stat_active, -1);
}

ssize_t posix_send(connection_t *c, u_char *buf, size_t size)
{
    ssize_t n;
    event_t *wev;

    wev = c->write;

    for (;;) {
        n = send(c->fd, buf, size, 0);
        if (n > 0) {
            if (n < (ssize_t)size) {
                wev->ready = 0;
            }

            c->sent += n;

            return n;
        }

        if (n == 0) {
            wev->ready = 0;
            return n;
        }

        if (errno == EAGAIN || errno == EINTR) {
            wev->ready = 0;

            if (errno == EAGAIN) {
                return -2;
            }

        } else {
            wev->error = 1;
            return -1;
        }
    }
}

ssize_t posix_recv(connection_t *c, u_char *buf, size_t size)
{
    ssize_t n;
    event_t *rev;
    rev = c->read;
    if (!rev->available && !rev->pending_eof) {
        rev->ready = 0;
        return -2;
    }

    do {
        n = recv(c->fd, buf, size, 0);
        if (n == 0) {
            rev->ready = 0;
            rev->eof = 1;
            return 0;
        }

        if (n > 0) {
            if (use_epoll_rdhup) {
                if ((size_t)n < size) {
                    if (!rev->pending_eof) {
                        rev->ready = 0;
                    }
                    rev->available = 0;
                }
            }
            return n;
        }

        if (errno == EAGAIN || errno == EINTR) {
            n = -2;
        } else {
            n = -1;
            break;
        }

    } while (errno == EINTR);

    rev->ready = 0;

    if (n == -1) {
        rev->error = 1;
    }
    return n;
}
