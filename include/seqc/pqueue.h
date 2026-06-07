#pragma once

#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"
#include "seqc/vec.h"

/* Binary min-heap priority queue.
 *
 * The element with the lowest compare_fn value is always at the front.
 * Pass a negated comparator to get max-heap behaviour.
 *
 * compare_fn: same type as iter_sort — negative / zero / positive. */

typedef struct pqueue_t pqueue_t;

pqueue_t *pqueue_create(size_t elem_size, compare_fn cmp, allocator_t allocator);

/* Build a heap from a copy of v's elements in O(n) using Floyd's algorithm.
 * The vec_t's element size must match elem_size; behaviour is undefined
 * otherwise. The returned pqueue_t owns its own allocation independent of v. */
pqueue_t *pqueue_build_from_vec(
    const vec_t *v, compare_fn cmp, allocator_t allocator);

/* Push a copy of elem and restore the heap property. */
seqc_status_t pqueue_push(pqueue_t *q, const void *elem);

/* Copy the minimum element to *out (may be NULL to discard) and remove it.
 * Returns SEQC_OK on success, SEQC_NOT_FOUND if the queue is empty. */
seqc_status_t pqueue_pop(pqueue_t *q, void *out);

/* Copy the minimum element into out (may be NULL to test for non-empty).
 * Returns SEQC_OK if non-empty, SEQC_NOT_FOUND if empty.
 * Invalidated by pqueue_push (may reallocate the backing buffer) or
 * pqueue_pop (reorders the heap). */
seqc_status_t pqueue_peek(const pqueue_t *q, void *out);

size_t pqueue_len(const pqueue_t *q);
bool pqueue_is_empty(const pqueue_t *q);
iter_t pqueue_iter(
    const pqueue_t *q); /* heap-storage order (unspecified priority order) */
iter_t pqueue_iter_rev(const pqueue_t *q); /* reverse heap-storage order */
void pqueue_clear(pqueue_t *q);          /* empty the queue, keep buffer */
void pqueue_free(pqueue_t *q);

/* Pop all elements in priority order into an allocator-owned slice_t.
 * The queue is empty after this call; the pqueue_t itself is not freed. */
slice_t pqueue_drain(pqueue_t *q, allocator_t allocator);
