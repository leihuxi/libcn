#include "connection.h"

static void drain_connections(cycle_t *cycle);

listening_t *create_listening(cycle_t *cycle, struct sockaddr *sockaddr,
                              socklen_t socklen)
{
    size_t len;
    listening_t *ls;
    struct sockaddr *sa;

    ls = palloc(cycle->pool, sizeof(listening_t));
    if (ls == NULL) {
        return NULL;
    }

    memset(ls, 0, sizeof(listening_t));

    sa = palloc(cycle->pool, socklen);
    if (sa == NULL) {
        return NULL;
    }

    memcpy(sa, sockaddr, socklen);
    cycle->listening = ls;
    ls->pool_size = 256;
    ls->sockaddr = sa;
    ls->socklen = socklen;
    ls->fd = (int)-1;
    ls->type = SOCK_STREAM;
    ls->backlog = LISTEN_BACKLOG;
    ls->rcvbuf = -1;
    ls->sndbuf = -1;
    ls->add_deferred = 1;
    ls->fastopen = 1;
    ls->keepalive = 1;
    ls->keepidle = 7200;
    ls->keepintvl = 75;
    ls->keepcnt = 9;
    ls->post_accept_timeout = 300;

    return ls;
}


int32_t open_listening_sockets(cycle_t *cycle)
{
    int reuseaddr;
    uint32_t i, tries;
    int s;
    listening_t *ls;
    int nb;
    nb = 1;
    reuseaddr = 1;

    ls = cycle->listening;

    if (ls->ignore) {
        return -1;
    }


    if (ls->add_reuseport) {

        /*
         * to allow transition from a socket without SO_REUSEPORT
         * to multiple sockets with SO_REUSEPORT, we have to set
         * SO_REUSEPORT on the old socket before opening new ones
         */
        int reuseport = 1;
        if (setsockopt(ls->fd, SOL_SOCKET, SO_REUSEPORT,
                       (const void *)&reuseport, sizeof(int))
            == -1) {
        }
        ls->add_reuseport = 0;
    }

    if (ls->fd != (int)-1) {
        return -1;
    }

    s = socket(ls->sockaddr->sa_family, ls->type, 0);

    if (s == (int)-1) {
        return -1;
    }

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuseaddr,
                   sizeof(int))
        == -1) {

        if (close(s) == -1) {
        }

        return -1;
    }

    if (ls->reuseport) {
        int reuseport;

        reuseport = 1;

        if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (const void *)&reuseport,
                       sizeof(int))
            == -1) {

            if (close(s) == -1) {
            }

            return -1;
        }
    }

    if (ioctl(s, FIONBIO, &nb) == -1) {
        return -1;
    }

    if (bind(s, ls->sockaddr, ls->socklen) == -1) {

        if (close(s) == -1) {
        }

        if (errno != EADDRINUSE) {
        }
        return -1;
    }

    if (listen(s, ls->backlog) == -1) {

        /*
         * on OpenVZ after suspend/resume EADDRINUSE
         * may be returned by listen() instead of bind(), see
         * https://bugzilla.openvz.org/show_bug.cgi?id=2470
         */

        if (close(s) == -1) {
        }

        if (errno != EADDRINUSE) {
        }

        return -1;
    }

    ls->listen = 1;
    ls->fd = s;
    return 0;
}


void configure_listening_sockets(cycle_t *cycle)
{
    int value;
    uint32_t i;
    listening_t *ls;

    ls = cycle->listening;

    if (ls->rcvbuf != -1) {
        if (setsockopt(ls->fd, SOL_SOCKET, SO_RCVBUF, (const void *)&ls->rcvbuf,
                       sizeof(int))
            == -1) {
        }
    }

    if (ls->sndbuf != -1) {
        if (setsockopt(ls->fd, SOL_SOCKET, SO_SNDBUF,
                       (const void *)&(ls->sndbuf), sizeof(int))
            == -1) {
        }
    }

    if (ls->keepalive) {
        value = (ls->keepalive == 1) ? 1 : 0;

        if (setsockopt(ls->fd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&value,
                       sizeof(int))
            == -1) {
        }
    }

    if (ls->keepidle) {
        value = ls->keepidle;
        if (setsockopt(ls->fd, IPPROTO_TCP, TCP_KEEPIDLE, (const void *)&value,
                       sizeof(int))
            == -1) {
        }
    }

    if (ls->keepintvl) {
        value = ls->keepintvl;
        if (setsockopt(ls->fd, IPPROTO_TCP, TCP_KEEPINTVL, (const void *)&value,
                       sizeof(int))
            == -1) {
        }
    }

    if (ls->keepcnt) {
        value = ls->keepcnt;
        if (setsockopt(ls->fd, IPPROTO_TCP, TCP_KEEPCNT, (const void *)&value,
                       sizeof(int))
            == -1) {
        }
    }

    if (ls->fastopen != -1) {
        if (setsockopt(ls->fd, IPPROTO_TCP, TCP_FASTOPEN,
                       (const void *)&ls->fastopen, sizeof(int))
            == -1) {
        }
    }

    if (ls->listen) {

        /* change backlog via listen() */

        if (listen(ls->fd, ls->backlog) == -1) {
        }
    }

    if (ls->add_deferred || ls->delete_deferred) {
        value = ls->add_deferred ? 1 : 0;
        if (setsockopt(ls->fd, IPPROTO_TCP, TCP_DEFER_ACCEPT,
                       (const void *)&value, sizeof(int))
            == -1) {
        }
    }

    if (ls->add_deferred) {
        ls->deferred_accept = 1;
    }
    return;
}


void close_listening_sockets(cycle_t *cycle)
{
    uint32_t i;
    listening_t *ls;
    connection_t *c;

    accept_mutex_held = 0;
    use_accept_mutex = 0;

    ls = cycle->listening;

    c = ls->connection;

    if (c) {
        if (c->read->active) {
            epoll_del_event(c->read, READ_EVENT, 1);
        }

        free_connection(c);
        c->fd = (int)-1;
    }


    if (close(ls->fd) == -1) {
    }

    ls->fd = (int)-1;
}


connection_t *get_connection(int s, cycle_t *cycle)
{
    uint32_t instance;
    event_t *rev, *wev;
    connection_t *c;

    /* disable warning: Win32 SOCKET is u_int while UNIX socket is int */
    if (cycle->files && (uint32_t)s >= cycle->files_n) {
        return NULL;
    }

    c = cycle->free_connections;

    if (c == NULL) {
        drain_connections((cycle_t *)cycle);
        c = cycle->free_connections;
    }

    if (c == NULL) {
        return NULL;
    }

    cycle->free_connections = c->data;
    cycle->free_connection_n--;

    if (cycle->files && cycle->files[s] == NULL) {
        cycle->files[s] = c;
    }

    rev = c->read;
    wev = c->write;

    memset(c, 0, sizeof(connection_t));

    c->read = rev;
    c->write = wev;
    c->fd = s;

    instance = rev->instance;

    memset(rev, 0, sizeof(event_t));
    memset(wev, 0, sizeof(event_t));

    rev->instance = !instance;
    wev->instance = !instance;

    rev->data = c;
    wev->data = c;

    wev->write = 1;

    return c;
}


void free_connection(connection_t *c)
{
    c->data = cycle->free_connections;
    cycle->free_connections = c;
    cycle->free_connection_n++;

    if (cycle->files && cycle->files[c->fd] == c) {
        cycle->files[c->fd] = NULL;
    }
}


void close_connection(connection_t *c)
{
    uint32_t log_error, level;
    int fd;

    if (c->fd == (int)-1) {
        return;
    }

    if (c->read->timer_set) {
        event_del_timer(c->read);
    }

    if (c->write->timer_set) {
        event_del_timer(c->write);
    }

    if (!c->shared) {
        epoll_del_connection(c, CLOSE_EVENT);
    }

    if (c->read->posted) {
        delete_posted_event(c->read);
    }

    if (c->write->posted) {
        delete_posted_event(c->write);
    }

    c->read->closed = 1;
    c->write->closed = 1;

    reusable_connection(c, 0);

    free_connection(c);

    fd = c->fd;
    c->fd = (int)-1;

    if (close(fd) == -1) {
    }
}


void reusable_connection(connection_t *c, uint32_t reusable)
{
    if (c->reusable) {
        queue_remove(&c->queue);
        cycle->reusable_connections_n--;
    }

    c->reusable = reusable;

    if (reusable) {
        /* need cast as cycle is volatile */
        queue_insert_head((queue_t *)&cycle->reusable_connections_queue,
                          &c->queue);
        cycle->reusable_connections_n++;
    }
}


static void drain_connections(cycle_t *cycle)
{
    uint32_t i, n;
    queue_t *q;
    connection_t *c;

    n = c_max(c_min(32, cycle->reusable_connections_n / 8), 1);

    for (i = 0; i < n; i++) {
        if (queue_empty(&cycle->reusable_connections_queue)) {
            break;
        }

        q = queue_last(&cycle->reusable_connections_queue);
        c = queue_data(q, connection_t, queue);

        c->close = 1;
        c->read->handler(c->read);
    }
}


void close_idle_connections(cycle_t *cycle)
{
    uint32_t i;
    connection_t *c;

    c = cycle->connections;

    for (i = 0; i < cycle->connection_n; i++) {

        /* THREAD: lock */

        if (c[i].fd != (int)-1 && c[i].idle) {
            c[i].close = 1;
            c[i].read->handler(c[i].read);
        }
    }
}

int32_t send_lowat(connection_t *c, size_t lowat)
{
    int sndlowat;
    if (lowat == 0 || c->sndlowat) {
        return 0;
    }
    sndlowat = (int)lowat;
    if (setsockopt(c->fd, SOL_SOCKET, SO_SNDLOWAT, (const void *)&sndlowat,
                   sizeof(int))
        == -1) {
        return -1;
    }
    c->sndlowat = 1;
    return 0;
}
