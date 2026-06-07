#pragma once

#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* ringbuf_t — double-ended circular buffer
 *
 * All push/pop operations are amortised O(1).
 * Random access via ringbuf_at is O(1).
 * Iteration is front-to-back by default.
 *
 * Both ends are equally cheap to use, making it suitable as a deque,
 * sliding-window buffer, or FIFO/LIFO depending on which ends are used.
 */
typedef struct ringbuf_t ringbuf_t;

ringbuf_t *ringbuf_create(size_t elem_size, allocator_t allocator);

/* Push to back (tail) */
seqc_status_t ringbuf_push_back(ringbuf_t *r, const void *elem);
/* Push to front (head) */
seqc_status_t ringbuf_push_front(ringbuf_t *r, const void *elem);
/* Pop from front; copies element into out (may be NULL to discard).
 * Returns SEQC_NOT_FOUND if empty. */
seqc_status_t ringbuf_pop_front(ringbuf_t *r, void *out);
/* Pop from back; copies element into out (may be NULL to discard).
 * Returns SEQC_NOT_FOUND if empty. */
seqc_status_t ringbuf_pop_back(ringbuf_t *r, void *out);

/* O(1) indexed access — copies element at logical index i into *out.
 * Index 0 is the front element. out may be NULL to probe bounds only.
 * Returns SEQC_OK, or SEQC_NOT_FOUND if i >= len. */
seqc_status_t ringbuf_at(const ringbuf_t *r, size_t i, void *out);

size_t ringbuf_len(const ringbuf_t *r);
size_t ringbuf_cap(const ringbuf_t *r);
bool ringbuf_is_empty(const ringbuf_t *r);

iter_t ringbuf_iter(const ringbuf_t *r);     /* front→back */
iter_t ringbuf_iter_rev(const ringbuf_t *r); /* back→front */

void ringbuf_clear(ringbuf_t *r); /* empty buffer, keep allocation */
void ringbuf_free(ringbuf_t *r);
