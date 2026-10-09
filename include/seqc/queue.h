#pragma once

#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* queue_t — FIFO ring buffer */
typedef struct queue_t queue_t;

queue_t *queue_create(size_t elem_size, allocator_t allocator);
seqc_status_t queue_push(queue_t *q, const void *elem); /* enqueue at back */
seqc_status_t queue_pop(
    queue_t *q, void *out); /* dequeue from front; SEQC_NOT_FOUND if empty */
/* The front element (what queue_pop would return) / the back element (the
 * last pushed).  Copies into out (may be NULL to test only), so it is not
 * affected by later changes.  SEQC_NOT_FOUND if empty. */
seqc_status_t queue_front(const queue_t *q, void *out);
seqc_status_t queue_back(const queue_t *q, void *out);
/* Pointers to the front / back element, NULL if empty — valid only until the
 * next change to the queue. */
void *queue_front_ptr(const queue_t *q);
void *queue_back_ptr(
    const queue_t *q); /* back (last enqueued) element; NULL if empty */
bool queue_is_empty(const queue_t *q);
size_t queue_len(const queue_t *q);
iter_t queue_iter(const queue_t *q);     /* front→back */
iter_t queue_iter_rev(const queue_t *q); /* back→front */
/* Push every element of it (consumed) at the back; stops at the first
 * error. */
seqc_status_t queue_extend(queue_t *q, iter_t it);
void queue_clear(queue_t *q); /* empty the queue, keep buffer */
void queue_destroy(queue_t *q);
