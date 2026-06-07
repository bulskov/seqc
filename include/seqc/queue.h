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
void *queue_peek(const queue_t *q); /* front element; NULL if empty */
void *queue_back(
    const queue_t *q); /* back (last enqueued) element; NULL if empty */
bool queue_is_empty(const queue_t *q);
size_t queue_len(const queue_t *q);
iter_t queue_iter(const queue_t *q);     /* front→back */
iter_t queue_iter_rev(const queue_t *q); /* back→front */
void queue_clear(queue_t *q);          /* empty the queue, keep buffer */
void queue_free(queue_t *q);
