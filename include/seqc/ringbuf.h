#pragma once

#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* RingBuf — double-ended circular buffer
 *
 * All push/pop operations are amortised O(1).
 * Random access via ringbuf_at is O(1).
 * Iteration is front-to-back by default.
 *
 * Both ends are equally cheap to use, making it suitable as a deque,
 * sliding-window buffer, or FIFO/LIFO depending on which ends are used.
 */
typedef struct RingBuf RingBuf;

RingBuf *ringbuf_create(size_t elem_size, allocator_t allocator);

/* Push to back (tail) */
SeqcStatus ringbuf_push_back(RingBuf *r, const void *elem);
/* Push to front (head) */
SeqcStatus ringbuf_push_front(RingBuf *r, const void *elem);
/* Pop from front; copies element into out (may be NULL to discard).
 * Returns SEQC_NOT_FOUND if empty. */
SeqcStatus ringbuf_pop_front(RingBuf *r, void *out);
/* Pop from back; copies element into out (may be NULL to discard).
 * Returns SEQC_NOT_FOUND if empty. */
SeqcStatus ringbuf_pop_back(RingBuf *r, void *out);

/* O(1) indexed access — copies element at logical index i into *out.
 * Index 0 is the front element. out may be NULL to probe bounds only.
 * Returns SEQC_OK, or SEQC_NOT_FOUND if i >= len. */
SeqcStatus ringbuf_at(const RingBuf *r, size_t i, void *out);

size_t ringbuf_len(const RingBuf *r);
size_t ringbuf_cap(const RingBuf *r);
bool ringbuf_is_empty(const RingBuf *r);

Iter ringbuf_iter(const RingBuf *r);     /* front→back */
Iter ringbuf_iter_rev(const RingBuf *r); /* back→front */

void ringbuf_clear(RingBuf *r); /* empty buffer, keep allocation */
void ringbuf_free(RingBuf *r);
