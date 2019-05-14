#include "times.h"

static rbtree_key_t monotonic_time(time_t sec, uint64_t msec);


/*
 * The time may be updated by signal handler or by several threads.
 * The time update operations are rare and require to hold the time_lock.
 * The time read operations are frequent, so they are lock-free and get time
 * values and strings from the current slot.  Thus thread may get the corrupted
 * values only if it is preempted while copying and then it is not scheduled
 * to run more than TIME_SLOTS seconds.
 */

#define TIME_SLOTS 64

static uint64_t slot;
static atomic_t time_lock;

volatile rbtree_key_t current_msec;
volatile stime_t *cached_time;

/*
 * localtime() and localtime_r() are not Async-Signal-Safe functions, therefore,
 * they must not be called by a signal handler, so we use the cached
 * GMT offset value. Fortunately the value is changed only two times a year.
 */

static int64_t cached_gmtoff;

static stime_t scached_time[TIME_SLOTS];


void stime_init(void)
{
    cached_time = &scached_time[0];
    stime_update();
}


void stime_update(void)
{
    u_char *p0, *p1;
    struct tm tm, gmt;
    time_t sec;
    uint64_t msec;
    stime_t *tp;
    struct timeval tv;

    if (!trylock(&time_lock)) {
        return;
    }

    gettimeofday(&tv, NULL);

    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;

    current_msec = monotonic_time(sec, msec);

    tp = &scached_time[slot];

    if (tp->sec == sec) {
        tp->msec = msec;
        unlock(&time_lock);
        return;
    }

    if (slot == TIME_SLOTS - 1) {
        slot = 0;
    } else {
        slot++;
    }

    tp = &scached_time[slot];

    tp->sec = sec;
    tp->msec = msec;

    sgmtime(sec, &gmt);

    memory_barrier();
    cached_time = tp;
    unlock(&time_lock);
}


static rbtree_key_t monotonic_time(time_t sec, uint64_t msec)
{
    struct timespec ts;

#if defined(CLOCK_MONOTONIC_FAST)
    clock_gettime(CLOCK_MONOTONIC_FAST, &ts);
    sec = ts.tv_sec;
    msec = ts.tv_nsec / 1000000;
#elif defined(CLOCK_MONOTONIC_COARSE)
    clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    sec = ts.tv_sec;
    msec = ts.tv_nsec / 1000000;
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
    sec = ts.tv_sec;
    msec = ts.tv_nsec / 1000000;
#endif

    return (rbtree_key_t)sec * 1000 + msec;
}

void sgmtime(time_t t, struct tm *tp)
{
    int64_t yday;
    uint64_t sec, min, hour, mday, mon, year, wday, days, leap;

    /* the calculation is valid for positive time_t only */

    if (t < 0) {
        t = 0;
    }

    days = t / 86400;
    sec = t % 86400;

    /*
     * no more than 4 year digits supported,
     * truncate to December 31, 9999, 23:59:59
     */

    if (days > 2932896) {
        days = 2932896;
        sec = 86399;
    }

    /* January 1, 1970 was Thursday */

    wday = (4 + days) % 7;

    hour = sec / 3600;
    sec %= 3600;
    min = sec / 60;
    sec %= 60;

    /*
     * the algorithm based on Gauss' formula,
     * see src/core/parse_time.c
     */

    /* days since March 1, 1 BC */
    days = days - (31 + 28) + 719527;

    /*
     * The "days" should be adjusted to 1 only, however, some March 1st's go
     * to previous year, so we adjust them to 2.  This causes also shift of the
     * last February days to next year, but we catch the case when "yday"
     * becomes negative.
     */

    year = (days + 2) * 400 / (365 * 400 + 100 - 4 + 1);

    yday = days - (365 * year + year / 4 - year / 100 + year / 400);

    if (yday < 0) {
        leap = (year % 4 == 0) && (year % 100 || (year % 400 == 0));
        yday = 365 + leap + yday;
        year--;
    }

    /*
     * The empirical formula that maps "yday" to month.
     * There are at least 10 variants, some of them are:
     *     mon = (yday + 31) * 15 / 459
     *     mon = (yday + 31) * 17 / 520
     *     mon = (yday + 31) * 20 / 612
     */

    mon = (yday + 31) * 10 / 306;

    /* the Gauss' formula that evaluates days before the month */

    mday = yday - (367 * mon / 12 - 30) + 1;

    if (yday >= 306) {

        year++;
        mon -= 10;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday -= 306;
         */

    } else {

        mon += 2;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday += 31 + 28 + leap;
         */
    }

    tp->tm_sec = (int)sec;
    tp->tm_min = (int)min;
    tp->tm_hour = (int)hour;
    tp->tm_mday = (int)mday;
    tp->tm_mon = (int)mon;
    tp->tm_year = (int)year;
    tp->tm_wday = (int)wday;
}
