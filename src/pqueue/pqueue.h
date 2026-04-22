#pragma once

#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"
#include "vec/vec.h"

/* Binary min-heap priority queue.
 *
 * The element with the lowest compare_fn value is always at the front.
 * Pass a negated comparator to get max-heap behaviour.
 *
 * compare_fn: same type as iter_sort — negative / zero / positive. */

typedef struct {
  Vec data; /* flat array storage */
  compare_fn cmp;
} PQueue;

PQueue pqueue_create(size_t elem_size, compare_fn cmp, Allocator allocator);

/* Push a copy of elem and restore the heap property. */
void pqueue_push(PQueue *q, const void *elem);

/* Copy the minimum element to *out (may be NULL to discard) and remove it.
 * Returns 1 on success, 0 if the queue is empty. */
bool pqueue_pop(PQueue *q, void *out);

/* Return a pointer to the minimum element without removing it.
 * Returns NULL if the queue is empty. */
void *pqueue_peek(const PQueue *q);

size_t pqueue_len(const PQueue *q);
bool pqueue_is_empty(const PQueue *q);
void pqueue_free(PQueue *q);
