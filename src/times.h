#ifndef _TIMES_H_INCLUDED_
#define _TIMES_H_INCLUDED_
#include "core.h"

struct stime_s {
    time_t sec;
    uint64_t msec;
    int64_t gmtoff;
};

void stime_init(void);
void stime_update(void);
void stime_sigsafe_update(void);
void sgmtime(time_t t, struct tm *tp);

#define stime() cached_time->sec
#define stimeofday() (struct stime_t *)cached_time

/*
 * milliseconds elapsed since some unspecified point in the past
 * and truncated to msec_t, used in event timers
 */
extern volatile stime_t *cached_time;
extern volatile rbtree_key_t current_msec;

#endif /* _TIMES_H_INCLUDED_ */
