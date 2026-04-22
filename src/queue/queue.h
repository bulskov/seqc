#pragma once

#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* Queue — FIFO ring buffer */
typedef struct Queue Queue;

Queue *queue_create(size_t elem_size, Allocator allocator);
void queue_push(Queue *q, const void *elem); /* enqueue at back */
bool queue_pop(
    Queue *q, void *out);         /* dequeue from front; true=ok false=empty */
void *queue_peek(const Queue *q); /* front element; NULL if empty */
void *queue_back(
    const Queue *q); /* back (last enqueued) element; NULL if empty */
bool queue_is_empty(const Queue *q);
size_t queue_len(const Queue *q);
Iter queue_iter(const Queue *q);     /* front→back */
Iter queue_iter_rev(const Queue *q); /* back→front */
void queue_clear(Queue *q);          /* empty the queue, keep buffer */
void queue_free(Queue *q);
