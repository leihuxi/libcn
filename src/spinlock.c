#include <sched.h>
#include "atomic.h"

void spinlock(atomic_t *lock, atomic_int_t value, uint64_t spin, int32_t ncpu)
{
    uint64_t i, n;

    for (;;) {

        if (*lock == 0 && atomic_cmp_set(lock, 0, value)) {
            return;
        }

        if (ncpu > 1) {
            for (n = 1; n < spin; n <<= 1) {

                for (i = 0; i < n; i++) {
                    cpu_pause();
                }

                if (*lock == 0 && atomic_cmp_set(lock, 0, value)) {
                    return;
                }
            }
        }

        sched_yield();
    }
}
