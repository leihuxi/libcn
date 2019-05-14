#ifndef _ATOMIC_H_INCLUDED_
#define _ATOMIC_H_INCLUDED_
#include <unitypes.h>

/* GCC 4.1 builtin atomic operations */

#define HAVE_ATOMIC_OPS 1

typedef long atomic_int_t;
typedef unsigned long atomic_uint_t;

#define ATOMIC_T_LEN (sizeof("-9223372036854775808") - 1)

typedef volatile atomic_uint_t atomic_t;

#define atomic_cmp_set(lock, old, set)                                         \
    __sync_bool_compare_and_swap(lock, old, set)

#define atomic_fetch_add(value, add) __sync_fetch_and_add(value, add)

#define memory_barrier() __sync_synchronize()

#if (__i386__ || __i386 || __amd64__ || __amd64)
#define cpu_pause() __asm__("pause")
#else
#define cpu_pause()
#endif

void spinlock(atomic_t *lock, atomic_int_t value, uint64_t spin, int32_t ncpu);

#define trylock(lock) (*(lock) == 0 && atomic_cmp_set(lock, 0, 1))
#define unlock(lock) *(lock) = 0

#endif /* _ATOMIC_H_INCLUDED_ */
