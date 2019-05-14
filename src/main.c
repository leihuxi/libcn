#include "core.h"

int connection_n = 1024;
pid_t pid;
pid_t ppid;
#define INT32_LEN (sizeof("-2147483648") - 1)
#define INT64_LEN (sizeof("-9223372036854775808") - 1)
typedef void (*process_fun)(void *data);
socklen_t sfd;
#define dblog printf("%s:%d\n", __FILE__, __LINE__)

struct process {
    pid_t pid;
    int status;
    int channel[2];
    void *data;
    process_fun proc;
};
struct process workers[1024];
int last_process = 0;
int channel;

void mywrite(event_t *wev)
{
    connection_t *c = wev->data;
    size_t size;
    ssize_t n;
    buf_t *b;
    if (wev->timedout) {
        return;
    }

    if (c->close) {
        close_connection(c);
        return;
    }

    c = wev->data;
    b = c->buffer;
    if (b == NULL) {
        return;
    }
    size = b->last - b->pos;
    if (size <= 0) {
        return;
    }
    printf("%zu\n", size);
    do {
        n = c->send(c, b->pos, size);
        printf("send %zd\n", n);
        if (n == -1) {
            close_connection(c);
            return;
        }
        if (n == -2) {
            return;
        }
        if (n > 0) {
            if (wev->timer_set) {
                event_del_timer(wev);
            }
            if (n == size) {
                if (handle_read_event(wev) == -1) {
                }
                return;
            }
            b->pos += n;
        }
    } while (wev->ready);
}

void myread(event_t *rev)
{
    dblog;
    u_char *p;
    buf_t *b;
    size_t size;
    ssize_t n;
    connection_t *c;

    c = rev->data;

    if (rev->timedout) {
        return;
    }

    if (c->close) {
        close_connection(c);
        return;
    }

    dblog;
    size = 1024;
    b = c->buffer;
    if (b == NULL) {
        b = create_temp_buf(c->pool, size);
        if (b == NULL) {
            close_connection(c);
            return;
        }

        c->buffer = b;

    } else if (b->start == NULL) {
        b->start = palloc(c->pool, size);
        if (b->start == NULL) {
            close_connection(c);
            return;
        }

        b->pos = b->start;
        b->last = b->start;
        b->end = b->last + size;
    }

    do {
        n = c->recv(c, b->last, size);
        if (n == -2) {
            break;
        }

        if (n == -1) {
            close_connection(c);
            return;
        }

        b->last += n;
    } while (rev->ready);

    dblog;
    reusable_connection(c, 0);
    mywrite(rev);
}

void callback(connection_t *c)
{
    dblog;
    uint32_t i;
    event_t *rev;
    struct sockaddr_in *sin;

    rev = c->read;
    rev->handler = myread;
    c->write->handler = mywrite;

    if (rev->ready) {
        if (use_accept_mutex) {
            event_add_timer(rev, c->listening->post_accept_timeout);
            post_event(rev, &posted_events);
            return;
        }
        rev->handler(rev);
        return;
    }

    event_add_timer(rev, c->listening->post_accept_timeout);
    reusable_connection(c, 1);

    if (handle_read_event(rev) != 0) {
        close_connection(c);
        return;
    }
}

int daemon_internal()
{
    int fd;
    switch (fork()) {
    case -1:
        return -1;
    case 0:
        break;
    default:
        printf("pid exit:%d\n", getpid());
        exit(0);
    }

    ppid = pid;
    pid = getpid();

    if (setsid() == -1) {
        printf("setsid\n");
        return -1;
    }

    umask(0);

    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        return -1;
    }

    /*     if (dup2(fd, STDIN_FILENO) == -1) { */
    /*         return -1; */
    /*     } */

    /*     if (dup2(fd, STDOUT_FILENO) == -1) { */
    /*         return -1; */
    /*     } */

    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            return -1;
        }
    }
    return 0;
}

static int create_pidfile()
{
    int fd = open("./pid", O_RDWR, O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        printf("open file fail:%d\n", errno);
        return -1;
    }

    char pid_info[INT64_LEN + 2];
    int len = snprintf(pid_info, INT64_LEN + 1, "%d\n", pid);
    pid_info[len + 1] = 0;
    if (write(fd, pid_info, len) == -1) {
        printf("write file fail:%d\n", errno);
        return -1;
    }

    if (close(fd) == -1) {
        printf("write file fail:%d\n", errno);
    }
    return 0;
}

void do_work()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGWINCH);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
    }
    sigemptyset(&set);
}

void proc(void *data)
{
    sigset_t set;
    int n = *(int *)data;
    int i;
    sigemptyset(&set);
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
    }
    for (i = 0; i < last_process; ++i) {
        if (workers[i].pid == -1) {
            continue;
        }
        close(workers[i].channel[1]);
    }
    // TODO:init
    printf("%d worker from parent %d\n", getpid(), getppid());
    event_process_init(cycle);
    for (;;) {
        process_events_and_timers(cycle);
    }
}

pid_t spwan_process(void *i)
{
    int n = *(int *)i;
    int s;

    for (s = 0; s < n; ++s) {
        if (workers[s].pid == -1) {
            break;
        }
    }
    if (s == 1024) {
        return -1;
    }

    pid = fork();
    switch (pid) {
    case -1:
        close(workers[n].channel[0]);
        close(workers[n].channel[1]);
        return -1;
    case 0:
        ppid = pid;
        pid = getpid();
        proc(&n);
        break;
    default:
        break;
    }
    workers[n].pid = pid;
    /* workers[n].proc = epoll_process_event; */
    workers[n].data = &n;
    if (s == last_process) {
        last_process++;
    }
    return pid;
}

pid_t start_worker(int n)
{
    printf("start worker:pid:%d, ppid:%d\n", pid, ppid);
    int i;
    pid_t pid;
    for (i = 0; i < n; ++i) {
        workers[i].pid = -1;
    }
    for (i = 0; i < n; ++i) {
        spwan_process(&i);
    }
    return pid;
}

int main(int argc, char *const *argv)
{
    int wcnt = 1;
    struct sockaddr_in addr;
    socklen_t len;

    /* daemon_internal(); */
    printf("demaon\n");
    if (create_pidfile() != 0) {
        printf("create file fail\n");
        return 1;
    }

    len = sizeof(struct sockaddr_in);
    bzero((char *)&addr, len);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)8888);

    init_cycle();
    listening_t *ls = create_listening(cycle, (struct sockaddr *)&addr, len);
    ls->handler = callback;
    open_listening_sockets(cycle);
    printf("%s\n", "open listening");
    configure_listening_sockets(cycle);
    printf("%s\n", "configure listening");
    event_master_init(cycle);
    proc(&wcnt);
    /* start_worker(wcnt); */
    return 0;
}
