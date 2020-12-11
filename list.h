#ifndef _LIST_H_INCLUDED_
#define _LIST_H_INCLUDED_
#include <unitypes.h>
#include "core.h"

list_t *list_create(pool_t *pool, uint32_t n, size_t size);

/*
 *
 *  the iteration through the list:
 *
 *  part = &list.part;
 *  data = part->elts;
 *
 *  for (i = 0 ;; i++) {
 *
 *      if (i >= part->nelts) {
 *          if (part->next == NULL) {
 *              break;
 *          }
 *
 *          part = part->next;
 *          data = part->elts;
 *          i = 0;
 *      }
 *
 *      ...  data[i] ...
 *
 *  }
 */


void *list_push(list_t *list);
int32_t list_init(list_t *list, pool_t *pool, uint32_t n, size_t size);

#endif /* _LIST_H_INCLUDED_ */
