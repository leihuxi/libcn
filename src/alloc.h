#ifndef _ALLOC_H_INCLUDED_
#define _ALLOC_H_INCLUDED_
#include <stdlib.h>
#include <string.h>
void *alloc(size_t size);
void *zalloc(size_t size);
/*
 * Linux has memalign() or posix_memalign()
 * Solaris has memalign()
 * FreeBSD 7.0 has posix_memalign(), besides, early version's malloc()
 * aligns allocations bigger than page size at the page boundary
 */
void *memalign(size_t alignment, size_t size);
#endif /* _ALLOC_H_INCLUDED_ */
