#ifndef _ARRAY_H_INCLUDED_
#define _ARRAY_H_INCLUDED_
#include "core.h"
array_t *array_create(pool_t *p, uint32_t n, size_t size);
void array_destroy(array_t *a);
void *array_push(array_t *a);
void *array_push_n(array_t *a, uint32_t n);
int32_t array_init(array_t *array, pool_t *pool, uint32_t n, size_t size);

#endif /* _ARRAY_H_INCLUDED_ */
