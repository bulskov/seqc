#include "pqueue.h"

#include <string.h>

PQueue pqueue_create(size_t elem_size, compare_fn cmp, Allocator allocator) {
  return (PQueue){.data = vec_create(elem_size, allocator), .cmp = cmp};
}

/* ---- helpers ----------------------------------------------------------- */

static void swap_elems(void *a, void *b, size_t size) {
  char *ca = a, *cb = b;
  while (size--) {
    char tmp = *ca;
    *ca++ = *cb;
    *cb++ = tmp;
  }
}

static void sift_up(PQueue *q, size_t i) {
  while (i > 0) {
    size_t parent = (i - 1) / 2;
    if (q->cmp(vec_get(&q->data, i), vec_get(&q->data, parent)) < 0) {
      swap_elems(vec_get(&q->data, i), vec_get(&q->data, parent),
                 q->data.elem_size);
      i = parent;
    } else {
      break;
    }
  }
}

static void sift_down(PQueue *q, size_t i) {
  size_t n = q->data.len;
  while (1) {
    size_t smallest = i;
    size_t left = 2 * i + 1;
    size_t right = 2 * i + 2;
    if (left < n &&
        q->cmp(vec_get(&q->data, left), vec_get(&q->data, smallest)) < 0)
      smallest = left;
    if (right < n &&
        q->cmp(vec_get(&q->data, right), vec_get(&q->data, smallest)) < 0)
      smallest = right;
    if (smallest == i)
      break;
    swap_elems(vec_get(&q->data, i), vec_get(&q->data, smallest),
               q->data.elem_size);
    i = smallest;
  }
}

/* ---- public API -------------------------------------------------------- */

void pqueue_push(PQueue *q, const void *elem) {
  if (!q || !elem)
    return;
  vec_push(&q->data, elem);
  sift_up(q, q->data.len - 1);
}

int pqueue_pop(PQueue *q, void *out) {
  if (!q || q->data.len == 0)
    return 0;
  if (out)
    memcpy(out, vec_get(&q->data, 0), q->data.elem_size);
  size_t last = q->data.len - 1;
  if (last > 0)
    memcpy(vec_get(&q->data, 0), vec_get(&q->data, last), q->data.elem_size);
  q->data.len--;
  if (q->data.len > 0)
    sift_down(q, 0);
  return 1;
}

void *pqueue_peek(const PQueue *q) {
  if (!q || q->data.len == 0)
    return NULL;
  return vec_get(&q->data, 0);
}

size_t pqueue_len(const PQueue *q) { return q ? q->data.len : 0; }
int pqueue_is_empty(const PQueue *q) { return !q || q->data.len == 0; }

void pqueue_free(PQueue *q) {
  if (q)
    vec_free(&q->data);
}
