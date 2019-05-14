#ifndef _CONNECTION_H_INCLUDED_
#define _CONNECTION_H_INCLUDED_
#include "core.h"

#define DEFAULT_CONNECTIONS 512
#define LISTEN_BACKLOG 511

typedef void (*connection_handler_pt)(connection_t *c);
typedef ssize_t (*recv_pt)(connection_t *c, u_char *buf, size_t size);
typedef ssize_t (*send_pt)(connection_t *c, u_char *buf, size_t size);
typedef struct udp_connection_s udp_connection_t;

struct listening_s {
    int fd;

    struct sockaddr *sockaddr;
    socklen_t socklen; /* size of sockaddr */

    int type;

    int backlog;
    int rcvbuf;
    int sndbuf;

    int keepidle;
    int keepintvl;
    int keepcnt;
    /* handler of accepted connection */
    connection_handler_pt handler;

    void *servers; /* array of http_in_addr_t, for example */

    size_t pool_size;
    /* should be here because of the AcceptEx() preread */
    size_t post_accept_buffer_size;
    /* should be here because of the deferred accept */
    rbtree_key_t post_accept_timeout;

    listening_t *previous;
    connection_t *connection;

    rbtree_t rbtree;
    rbtree_node_t sentinel;

    uint32_t worker;

    unsigned open : 1;
    unsigned remain : 1;
    unsigned ignore : 1;

    unsigned nonblocking_accept : 1;
    unsigned listen : 1;
    unsigned nonblocking : 1;
    unsigned shared : 1;

    unsigned reuseport : 1;
    unsigned add_reuseport : 1;
    unsigned keepalive : 2;

    unsigned deferred_accept : 1;
    unsigned delete_deferred : 1;
    unsigned add_deferred : 1;
    int fastopen;
};


struct udp_connection_s {
    rbtree_node_t node;
    connection_t *connection;
    u_char *buffer;
};

struct connection_s {
    void *data;
    event_t *read;
    event_t *write;

    socklen_t fd;

    recv_pt recv;
    send_pt send;

    listening_t *listening;

    off_t sent;

    pool_t *pool;

    int type;

    struct sockaddr *sockaddr;
    socklen_t socklen;

    udp_connection_t *udp;

    struct sockaddr *local_sockaddr;
    socklen_t local_socklen;

    buf_t *buffer;

    queue_t queue;

    atomic_uint_t number;

    uint64_t requests;

    unsigned buffered : 8;

    unsigned timedout : 1;
    unsigned error : 1;
    unsigned destroyed : 1;

    unsigned idle : 1;
    unsigned reusable : 1;
    unsigned close : 1;
    unsigned shared : 1;

    unsigned sendfile : 1;
    unsigned sndlowat : 1;
    unsigned tcp_nodelay : 2; /* connection_tcp_nodelay_e */
    unsigned tcp_nopush : 2;  /* connection_tcp_nopush_e */

    unsigned need_last_buf : 1;
};

listening_t *create_listening(cycle_t *cycle, struct sockaddr *sockaddr,
                              socklen_t socklen);
int32_t open_listening_sockets(cycle_t *cycle);
void configure_listening_sockets(cycle_t *cycle);
void close_listening_sockets(cycle_t *cycle);
void close_connection(connection_t *c);
void close_idle_connections(cycle_t *cycle);
int32_t tcp_nodelay(connection_t *c);

connection_t *get_connection(int s, cycle_t *cycle);
void free_connection(connection_t *c);
void reusable_connection(connection_t *c, uint32_t reusable);

ssize_t posix_recv(connection_t *c, u_char *buf, size_t size);
ssize_t posix_send(connection_t *c, u_char *buf, size_t size);
int32_t send_lowat(connection_t *c, size_t lowat);
#endif /* _CONNECTION_H_INCLUDED_ */
