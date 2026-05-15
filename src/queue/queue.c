#include "seqc/queue.h"

#include <stdbool.h>
#include <string.h>

#define INITIAL_CAP 16

struct Queue
{
    char *buf;
    size_t cap;
    size_t len;
    size_t head; /* index of front element */
    size_t elem_size;
    allocator_t allocator;
};

Queue *queue_create(size_t elem_size, allocator_t allocator)
{
    Queue *q = mem_alloc(allocator, sizeof(Queue), _Alignof(Queue));
    if (!q)
        return NULL;
    *q = (Queue){.buf = NULL,
                 .cap = 0,
                 .len = 0,
                 .head = 0,
                 .elem_size = elem_size,
                 .allocator = allocator};
    return q;
}

static SeqcStatus queue_grow(Queue *q)
{
    size_t new_cap = q->cap == 0 ? INITIAL_CAP : q->cap * 2;
    char *new_buf = mem_alloc(q->allocator,
         new_cap * q->elem_size, _Alignof(max_align_t));
    if (!new_buf)
        return SEQC_OOM;
    /* copy elements from head to tail in logical order */
    for (size_t i = 0; i < q->len; i++)
    {
        size_t src = (q->head + i) % (q->cap == 0 ? 1 : q->cap);
        memcpy(
            new_buf + i * q->elem_size,
            q->buf + src * q->elem_size,
            q->elem_size);
    }
    if (q->buf)
        mem_free(q->allocator, q->buf, q->cap * q->elem_size);
    q->buf = new_buf;
    q->cap = new_cap;
    q->head = 0;
    return SEQC_OK;
}

SeqcStatus queue_push(Queue *q, const void *elem)
{
    if (!q || !elem)
        return SEQC_INVALID;
    if (q->len == q->cap)
    {
        SeqcStatus s = queue_grow(q);
        if (s != SEQC_OK)
            return s;
    }
    size_t tail = (q->head + q->len) % q->cap;
    memcpy(q->buf + tail * q->elem_size, elem, q->elem_size);
    q->len++;
    return SEQC_OK;
}

SeqcStatus queue_pop(Queue *q, void *out)
{
    if (!q || q->len == 0)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, q->buf + q->head * q->elem_size, q->elem_size);
    q->head = (q->head + 1) % q->cap;
    q->len--;
    return SEQC_OK;
}

void *queue_peek(const Queue *q)
{
    if (!q || q->len == 0)
        return NULL;
    return q->buf + q->head * q->elem_size;
}

void *queue_back(const Queue *q)
{
    if (!q || q->len == 0)
        return NULL;
    size_t tail = (q->head + q->len - 1) % q->cap;
    return q->buf + tail * q->elem_size;
}

bool queue_is_empty(const Queue *q)
{
    return !q || q->len == 0;
}

size_t queue_len(const Queue *q)
{
    return q ? q->len : 0;
}

void queue_clear(Queue *q)
{
    if (q)
    {
        q->len = 0;
        q->head = 0;
    }
}

void queue_free(Queue *q)
{
    if (!q)
        return;
    if (q->buf)
        mem_free(q->allocator, q->buf, q->cap * q->elem_size);
    allocator_t al = q->allocator;
    mem_free(al, q, sizeof(Queue));
}

/* ---- iter -------------------------------------------------------------- */

typedef struct
{
    const char *buf;
    size_t head;
    size_t cap;
    size_t remaining;
    size_t elem_size;
} QueueIterState;

static bool queue_iter_next(Iter *it, void *out)
{
    QueueIterState *s = it->state;
    if (s->remaining == 0 || s->cap == 0)
        return false;
    memcpy(out, s->buf + s->head * s->elem_size, s->elem_size);
    s->head = (s->head + 1) % s->cap;
    s->remaining--;
    return true;
}

static void queue_iter_drop(Iter *it)
{
    mem_free(it->allocator, it->state, sizeof(QueueIterState));
}

Iter queue_iter(const Queue *q)
{
    if (!q)
        return (Iter){0};
    QueueIterState *s = mem_alloc(q->allocator,
         sizeof *s, _Alignof(QueueIterState));
    *s = (QueueIterState){q->buf, q->head, q->cap, q->len, q->elem_size};
    return (Iter){.next = queue_iter_next,
                  .drop = queue_iter_drop,
                  .state = s,
                  .elem_size = q->elem_size,
                  .allocator = q->allocator};
}

/* ---- iter_rev ---------------------------------------------------------- */

typedef struct
{
    const char *buf;
    size_t tail; /* index of the next element to yield (walking backward) */
    size_t cap;
    size_t remaining;
    size_t elem_size;
} QueueIterRevState;

static bool queue_iter_rev_next(Iter *it, void *out)
{
    QueueIterRevState *s = it->state;
    if (s->remaining == 0 || s->cap == 0)
        return false;
    memcpy(out, s->buf + s->tail * s->elem_size, s->elem_size);
    s->tail = (s->tail + s->cap - 1) % s->cap;
    s->remaining--;
    return true;
}

static void queue_iter_rev_drop(Iter *it)
{
    mem_free(it->allocator, it->state, sizeof(QueueIterRevState));
}

Iter queue_iter_rev(const Queue *q)
{
    if (!q)
        return (Iter){0};
    QueueIterRevState *s = mem_alloc(q->allocator,
         sizeof *s, _Alignof(QueueIterRevState));
    size_t tail = (q->len > 0) ? (q->head + q->len - 1) % q->cap : 0;
    *s = (QueueIterRevState){q->buf, tail, q->cap, q->len, q->elem_size};
    return (Iter){.next = queue_iter_rev_next,
                  .drop = queue_iter_rev_drop,
                  .state = s,
                  .elem_size = q->elem_size,
                  .allocator = q->allocator};
}
