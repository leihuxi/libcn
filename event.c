#include "event.h"

static uint64_t timer_resolution;
sig_atomic_t event_timer_alarm;

shmtx_t accept_mutex;
uint32_t use_accept_mutex;
uint32_t accept_events;
uint32_t accept_mutex_held;
rbtree_key_t accept_mutex_delay;
int32_t accept_disabled;

atomic_t *accept_mutex_ptr;
static atomic_t connection_counter0 = 1;
atomic_t *connection_counter = &connection_counter0;
static atomic_t stat_accepted0;
atomic_t *stat_accepted = &stat_accepted0;
static atomic_t stat_handled0;
atomic_t *stat_handled = &stat_handled0;
static atomic_t stat_requests0;
atomic_t *stat_requests = &stat_requests0;
static atomic_t stat_active0;
atomic_t *stat_active = &stat_active0;
static atomic_t stat_reading0;
atomic_t *stat_reading = &stat_reading0;
static atomic_t stat_writing0;
atomic_t *stat_writing = &stat_writing0;
static atomic_t stat_waiting0;
atomic_t *stat_waiting = &stat_waiting0;

/* 初始化事件模块 */
int32_t event_master_init(cycle_t *cycle)
{
    u_char *shared;
    size_t size, cl;
    u_char *shm;
    struct rlimit rlmt;

    timer_resolution = TIMER_INTERVAL;
    /* 获取当前进程所打开的最大文件描述符个数 */
    if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        return -1;
    }

    if (accept_mutex_ptr) {
        return 0;
    }

    cl = 128;

    size = cl    /* ngx_accept_mutex */
           + cl  /* ngx_connection_counter */
           + cl  /* ngx_stat_handled */
           + cl  /* ngx_stat_requests */
           + cl  /* ngx_stat_active */
           + cl  /* ngx_stat_reading */
           + cl  /* ngx_stat_writing */
           + cl; /* ngx_stat_waiting */


    if (shm_alloc(&shm, size) != 0) {
        return -1;
    }

    shared = shm;
    accept_mutex_ptr = (atomic_t *)shared;
    accept_mutex.spin = (uint64_t)-1;
    if (shmtx_create(&accept_mutex, (shmtx_sh_t *)shared, (u_char *)"") != 0) {
        return -1;
    }

    /* 初始化变量 */
    connection_counter = (atomic_t *)(shared + 1 * cl);
    (void)atomic_cmp_set(connection_counter, 0, 1);

    stat_accepted = (atomic_t *)(shared + 2 * cl);
    stat_handled = (atomic_t *)(shared + 3 * cl);
    stat_requests = (atomic_t *)(shared + 4 * cl);
    stat_active = (atomic_t *)(shared + 5 * cl);
    stat_reading = (atomic_t *)(shared + 6 * cl);
    stat_writing = (atomic_t *)(shared + 7 * cl);
    stat_waiting = (atomic_t *)(shared + 8 * cl);
    return 0;
}

void process_events_and_timers(cycle_t *cycle)
{
    uint32_t flags;
    rbtree_key_t timer, delta;

    if (timer_resolution) {
        timer = TIMER_INFINITE;
        flags = 0;

    } else {
        timer = event_find_timer();
        flags = UPDATE_TIME;
    }

    if (use_accept_mutex) {
        if (accept_disabled > 0) {
            accept_disabled--;

        } else {
            if (trylock_accept_mutex(cycle) == -1) {
                return;
            }

            if (accept_mutex_held) {
                flags |= POST_EVENTS;

            } else {
                if (timer == TIMER_INFINITE || timer > accept_mutex_delay) {
                    timer = accept_mutex_delay;
                }
            }
        }
    }

    delta = current_msec;

    printf("timer=%ld\n", timer);
    (void)epoll_process_events(cycle, timer, flags);

    delta = current_msec - delta;

    event_process_posted(cycle, &posted_accept_events);

    if (accept_mutex_held) {
        shmtx_unlock(&accept_mutex);
    }

    if (delta) {
        event_expire_timers();
    }

    event_process_posted(cycle, &posted_events);
}

static void timer_signal_handler(int signo)
{
    event_timer_alarm = 1;
}


int32_t event_process_init(cycle_t *cycle)
{
    uint32_t m, i;
    event_t *rev, *wev;
    listening_t *ls;
    connection_t *c, *next, *old;

    /**开accept lock**/
    use_accept_mutex = 1;
    accept_mutex_delay = ACCEPT_MUTEX_DELAY;
    accept_mutex_held = 0;

    queue_init(&posted_accept_events);
    queue_init(&posted_events);

    if (event_timer_init() == -1) {
        return -1;
    }

    if (epoll_init(cycle)) {
        exit(2);
    }

    if (timer_resolution) {
        struct sigaction sa;
        struct itimerval itv;

        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = timer_signal_handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGALRM, &sa, NULL) == -1) {
            return -1;
        }

        itv.it_interval.tv_sec = timer_resolution / 1000;
        itv.it_interval.tv_usec = (timer_resolution % 1000) * 1000;
        itv.it_value.tv_sec = timer_resolution / 1000;
        itv.it_value.tv_usec = (timer_resolution % 1000) * 1000;

        if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
        }
    }

    cycle->connections = alloc(sizeof(connection_t) * cycle->connection_n);
    if (cycle->connections == NULL) {
        return -1;
    }

    c = cycle->connections;

    cycle->read_events = alloc(sizeof(event_t) * cycle->connection_n);
    if (cycle->read_events == NULL) {
        return -1;
    }

    rev = cycle->read_events;
    for (i = 0; i < cycle->connection_n; i++) {
        rev[i].closed = 1;
        rev[i].instance = 1;
    }

    cycle->write_events = alloc(sizeof(event_t) * cycle->connection_n);
    if (cycle->write_events == NULL) {
        return -1;
    }

    wev = cycle->write_events;
    for (i = 0; i < cycle->connection_n; i++) {
        wev[i].closed = 1;
    }

    i = cycle->connection_n;
    next = NULL;

    do {
        i--;

        c[i].data = next;
        c[i].read = &cycle->read_events[i];
        c[i].write = &cycle->write_events[i];
        c[i].fd = (int)-1;
        next = &c[i];
    } while (i);

    cycle->free_connections = next;
    cycle->free_connection_n = cycle->connection_n;

    /* for each listening socket */
    ls = cycle->listening;
    c = get_connection(ls->fd, cycle);

    if (c == NULL) {
        return -1;
    }

    c->type = ls->type;

    c->listening = ls;
    ls->connection = c;

    rev = c->read;

    rev->accept = 1;
    rev->deferred_accept = ls->deferred_accept;

    /* rev->handler = (c->type == SOCK_STREAM) ? event_accept : event_recvmsg;
     */
    rev->handler = event_accept;

    if (ls->reuseport) {
        if (epoll_add_event(rev, READ_EVENT, 0) == -1) {
            return -1;
        }
        return 0;
    }

    if (use_accept_mutex) {
        return -1;
    }

    if (epoll_add_event(rev, READ_EVENT, 0) == -1) {
        return -1;
    }

    return 0;
}

int32_t handle_read_event(event_t *rev)
{
    if (!rev->active && !rev->ready) {
        if (epoll_add_event(rev, READ_EVENT, CLEAR_EVENT) == -1) {
            return -1;
        }
    }
    return 0;
}

int32_t handle_write_event(event_t *wev, size_t lowat)
{
    connection_t *c;
    int sndlowat;

    if (lowat) {
        c = wev->data;
        if (send_lowat(c, lowat) == -1) {
            return -1;
        }
    }

    if (!wev->active && !wev->ready) {
        if (epoll_add_event(wev, WRITE_EVENT,
                            CLEAR_EVENT | (lowat ? LOWAT_EVENT : 0))
            == -1) {
            return -1;
        }
    }
    return 0;
}
