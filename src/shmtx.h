#ifndef _SHMTX_H_INCLUDED_
#define _SHMTX_H_INCLUDED_
#include "core.h"

struct shmtx_sh_s {
    atomic_t lock;
    atomic_t wait;
};

struct shmtx_s {
    atomic_t *lock;
    atomic_t *wait;
    uint64_t semaphore;
    sem_t sem;
    int fd;
    u_char *name;
    uint64_t spin;
};


void shm_free(u_char *addr, size_t size);
int32_t shm_alloc(u_char **addr, size_t size);
int32_t shmtx_create(shmtx_t *mtx, shmtx_sh_t *addr, u_char *name);
void shmtx_destroy(shmtx_t *mtx);
uint32_t shmtx_trylock(shmtx_t *mtx);
void shmtx_lock(shmtx_t *mtx, int32_t ncpu);
void shmtx_unlock(shmtx_t *mtx);
uint32_t shmtx_force_unlock(shmtx_t *mtx);

#endif /* _SHMTX_H_INCLUDED_ */
