#include "shmtx.h"
#include "atomic.h"

static void shmtx_wakeup(shmtx_t *mtx);

int32_t shmtx_create(shmtx_t *mtx, shmtx_sh_t *addr, u_char *name)
{
    mtx->lock = &addr->lock;

    if (mtx->spin == (uint64_t)-1) {
        return 0;
    }

    mtx->spin = 2048;


    mtx->wait = &addr->wait;

    if (sem_init(&mtx->sem, 1, 0) == -1) {
    } else {
        mtx->semaphore = 1;
    }
    return 0;
}


void shmtx_destroy(shmtx_t *mtx)
{
    if (mtx->semaphore) {
        if (sem_destroy(&mtx->sem) == -1) {
        }
    }
}


uint32_t shmtx_trylock(shmtx_t *mtx)
{
    return (*mtx->lock == 0 && atomic_cmp_set(mtx->lock, 0, getpid()));
}


void shmtx_lock(shmtx_t *mtx, int32_t ncpu)
{
    uint32_t i, n;
    for (;;) {

        if (*mtx->lock == 0 && atomic_cmp_set(mtx->lock, 0, getpid())) {
            return;
        }

        if (ncpu > 1) {
            for (n = 1; n < mtx->spin; n <<= 1) {
                for (i = 0; i < n; i++) {
                    __asm__("pause");
                }

                if (*mtx->lock == 0 && atomic_cmp_set(mtx->lock, 0, getpid())) {
                    return;
                }
            }
        }

        if (mtx->semaphore) {
            (void)atomic_fetch_add(mtx->wait, 1);

            if (*mtx->lock == 0 && atomic_cmp_set(mtx->lock, 0, getpid())) {
                (void)atomic_fetch_add(mtx->wait, -1);
                return;
            }

            while (sem_wait(&mtx->sem) == -1) {
                if (errno != EINTR) {
                    break;
                }
            }
            continue;
        }
        sched_yield();
    }
}

void shmtx_unlock(shmtx_t *mtx)
{
    if (mtx->spin != (uint64_t)-1) {
    }

    if (atomic_cmp_set(mtx->lock, getpid(), 0)) {
        shmtx_wakeup(mtx);
    }
}


uint32_t shmtx_force_unlock(shmtx_t *mtx)
{
    if (atomic_cmp_set(mtx->lock, getpid(), 0)) {
        shmtx_wakeup(mtx);
        return 1;
    }

    return 0;
}

static void shmtx_wakeup(shmtx_t *mtx)
{
    uint64_t wait;

    if (!mtx->semaphore) {
        return;
    }

    for (;;) {

        wait = *mtx->wait;

        if ((uint64_t)wait <= 0) {
            return;
        }

        if (atomic_cmp_set(mtx->wait, wait, wait - 1)) {
            break;
        }
    }

    if (sem_post(&mtx->sem) == -1) {
    }
}

int32_t shm_alloc(u_char **addr, size_t size)
{
    *addr = (u_char *)mmap(NULL, size, PROT_READ | PROT_WRITE,
                          MAP_ANON | MAP_SHARED, -1, 0);

    if (addr == MAP_FAILED) {
        return -1;
    }

    return 0;
}

void shm_free(u_char *addr, size_t size)
{
    if (munmap((void *)addr, size) == -1) {
    }
}
