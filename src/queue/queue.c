#include "queue.h"

#include <stdbool.h>
#include <string.h>

#define INITIAL_CAP 16

Queue queue_create(size_t elem_size, Allocator allocator) {
  return (Queue){.buf = NULL,
                 .cap = 0,
                 .len = 0,
                 .head = 0,
                 .elem_size = elem_size,
                 .allocator = allocator};
}

static void queue_grow(Queue *q) {
  size_t new_cap = q->cap == 0 ? INITIAL_CAP : q->cap * 2;
  char *new_buf = q->allocator.alloc(q->allocator.ctx, new_cap * q->elem_size,
                                     _Alignof(max_align_t));
  /* copy elements from head to tail in logical order */
  for (size_t i = 0; i < q->len; i++) {
    size_t src = (q->head + i) % q->cap;
    memcpy(new_buf + i * q->elem_size, q->buf + src * q->elem_size,
           q->elem_size);
  }
  if (q->allocator.free && q->buf)
    q->allocator.free(q->allocator.ctx, q->buf);
  q->buf = new_buf;
  q->cap = new_cap;
  q->head = 0;
}

void queue_push(Queue *q, const void *elem) {
  if (!q || !elem)
    return;
  if (q->len == q->cap)
    queue_grow(q);
  size_t tail = (q->head + q->len) % q->cap;
  memcpy(q->buf + tail * q->elem_size, elem, q->elem_size);
  q->len++;
}

bool queue_pop(Queue *q, void *out) {
  if (!q || q->len == 0)
    return false;
  if (out)
    memcpy(out, q->buf + q->head * q->elem_size, q->elem_size);
  q->head = (q->head + 1) % q->cap;
  q->len--;
  return true;
}

void *queue_peek(const Queue *q) {
  if (!q || q->len == 0)
    return NULL;
  return q->buf + q->head * q->elem_size;
}

bool queue_is_empty(const Queue *q) { return !q || q->len == 0; }

size_t queue_len(const Queue *q) { return q ? q->len : 0; }

void queue_free(Queue *q) {
  if (!q || !q->buf)
    return;
  if (q->allocator.free)
    q->allocator.free(q->allocator.ctx, q->buf);
  q->buf = NULL;
  q->cap = 0;
  q->len = 0;
  q->head = 0;
}

/* ---- iter -------------------------------------------------------------- */

typedef struct {
  const char *buf;
  size_t head;
  size_t cap;
  size_t remaining;
  size_t elem_size;
} QueueIterState;

static bool queue_iter_next(Iter *it, void *out) {
  QueueIterState *s = it->state;
  if (s->remaining == 0 || s->cap == 0)
    return false;
  memcpy(out, s->buf + s->head * s->elem_size, s->elem_size);
  s->head = (s->head + 1) % s->cap;
  s->remaining--;
  return true;
}

static void queue_iter_drop(Iter *it) {
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, it->state);
}

Iter queue_iter(const Queue *q) {
  if (!q)
    return (Iter){0};
  QueueIterState *s =
      q->allocator.alloc(q->allocator.ctx, sizeof *s, _Alignof(QueueIterState));
  *s = (QueueIterState){q->buf, q->head, q->cap, q->len, q->elem_size};
  return (Iter){.next = queue_iter_next,
                .drop = queue_iter_drop,
                .state = s,
                .elem_size = q->elem_size,
                .allocator = q->allocator};
}
