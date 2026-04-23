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

typedef struct PQueue PQueue;

PQueue *pqueue_create(size_t elem_size, compare_fn cmp, Allocator allocator);

/* Build a heap from a copy of v's elements in O(n) using Floyd's algorithm.
 * The Vec's element size must match elem_size; behaviour is undefined
 * otherwise. The returned PQueue owns its own allocation independent of v. */
PQueue *pqueue_build_from_vec(const Vec *v, compare_fn cmp, Allocator allocator);

/* Push a copy of elem and restore the heap property. */
SeqcStatus pqueue_push(PQueue *q, const void *elem);

/* Copy the minimum element to *out (may be NULL to discard) and remove it.
 * Returns SEQC_OK on success, SEQC_NOT_FOUND if the queue is empty. */
SeqcStatus pqueue_pop(PQueue *q, void *out);

/* Copy the minimum element into out (may be NULL to test for non-empty).
 * Returns SEQC_OK if non-empty, SEQC_NOT_FOUND if empty.
 * Invalidated by pqueue_push (may reallocate the backing buffer) or
 * pqueue_pop (reorders the heap). */
SeqcStatus pqueue_peek(const PQueue *q, void *out);

size_t pqueue_len(const PQueue *q);
bool pqueue_is_empty(const PQueue *q);
Iter pqueue_iter(
    const PQueue *q); /* heap-storage order (unspecified priority order) */
Iter pqueue_iter_rev(const PQueue *q); /* reverse heap-storage order */
void pqueue_clear(PQueue *q);          /* empty the queue, keep buffer */
void pqueue_free(PQueue *q);
