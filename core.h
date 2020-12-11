#ifndef _CORE_INCLUDE_
#define _CORE_INCLUDE_
#include <unistd.h>
#include <sys/types.h>
#include <unitypes.h>
#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <netinet/tcp.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "common.h"

typedef struct cycle_s cycle_t;
typedef struct event_s event_t;
typedef struct connection_s connection_t;
typedef struct listening_s listening_t;

#include "event.h"
#include "connection.h"
#include "cycle.h"
#include "event_posted.h"
#include "event_timer.h"
#include "epoll_event.h"

#define c_abs(value) (((value) >= 0) ? (value) : -(value))
#define c_max(val1, val2) ((val1 < val2) ? (val2) : (val1))
#define c_min(val1, val2) ((val1 > val2) ? (val2) : (val1))

#endif /* ifndef _CORE_INCLUDE_ */
