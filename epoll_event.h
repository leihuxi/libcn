#ifndef _EPOLL_EVENT_H_
#define _EPOLL_EVENT_H_
#include "core.h"

#define EPOLL_EVETNS 1024

int32_t epoll_init(cycle_t *cycle);
void epoll_done(cycle_t *cycle);
int32_t epoll_add_event(event_t *ev, int32_t event, uint32_t flags);
int32_t epoll_del_event(event_t *ev, int32_t event, uint32_t flags);
int32_t epoll_add_connection(connection_t *c);
int32_t epoll_del_connection(connection_t *c, uint32_t flags);
int32_t epoll_process_events(cycle_t *cycle, rbtree_key_t timer,
                             uint32_t flags);
static int ep = -1;
static struct epoll_event *event_list;
static uint32_t nevents;
static uint32_t use_epoll_rdhup;

#define READ_EVENT (EPOLLIN | EPOLLRDHUP)
#define WRITE_EVENT EPOLLOUT
#define CLEAR_EVENT EPOLLET
#define LOWAT_EVENT 0

#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0
#endif /* ifndef EPOLLRDHUP */

#endif /* ifndef _EPOLL_EVENT_H_ */
