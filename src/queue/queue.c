#include "seqc/queue.h"
#include "collection.h"

#include "max_align.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define INITIAL_CAP 16

struct queue_t
{
    char *buf;
    size_t cap;
    size_t len;
    size_t head; /* index of front element */
    size_t elem_size;
    allocator_t allocator;
};

queue_t *queue_create(size_t elem_size, allocator_t allocator)
{
    queue_t *q = mem_alloc(allocator, sizeof(queue_t), _Alignof(queue_t));
    if (!q)
    {
        return NULL;
    }
    *q = (queue_t){
        .buf = NULL,
        .cap = 0,
        .len = 0,
        .head = 0,
        .elem_size = elem_size,
        .allocator = allocator};
    return q;
}

static seqc_status_t queue_grow(queue_t *q)
{
    if (q->cap > SIZE_MAX / 2)
    {
        return SEQC_OOM;
    }
    size_t new_cap = q->cap == 0 ? INITIAL_CAP : q->cap * 2;
    char *new_buf =
        mem_alloc(q->allocator, new_cap * q->elem_size, SEQC_MAX_ALIGN);
    if (!new_buf)
    {
        return SEQC_OOM;
    }
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
    {
        mem_free(q->allocator, q->buf, q->cap * q->elem_size);
    }
    q->buf = new_buf;
    q->cap = new_cap;
    q->head = 0;
    return SEQC_OK;
}

seqc_status_t queue_push(queue_t *q, const void *elem)
{
    if (!q || !elem)
    {
        return SEQC_INVALID;
    }
    if (q->len == q->cap)
    {
        seqc_status_t s = queue_grow(q);
        if (s != SEQC_OK)
        {
            return s;
        }
    }
    size_t tail = (q->head + q->len) % q->cap;
    memcpy(q->buf + tail * q->elem_size, elem, q->elem_size);
    q->len++;
    return SEQC_OK;
}

seqc_status_t queue_pop(queue_t *q, void *out)
{
    if (!q || q->len == 0)
    {
        return SEQC_NOT_FOUND;
    }
    if (out)
    {
        memcpy(out, q->buf + q->head * q->elem_size, q->elem_size);
    }
    q->head = (q->head + 1) % q->cap;
    q->len--;
    return SEQC_OK;
}

void *queue_front_ptr(const queue_t *q)
{
    if (!q || q->len == 0)
    {
        return NULL;
    }
    return q->buf + q->head * q->elem_size;
}

void *queue_back_ptr(const queue_t *q)
{
    if (!q || q->len == 0)
    {
        return NULL;
    }
    size_t tail = (q->head + q->len - 1) % q->cap;
    return q->buf + tail * q->elem_size;
}

bool queue_is_empty(const queue_t *q)
{
    return !q || q->len == 0;
}

size_t queue_len(const queue_t *q)
{
    return q ? q->len : 0;
}

void queue_clear(queue_t *q)
{
    if (q)
    {
        q->len = 0;
        q->head = 0;
    }
}

void queue_destroy(queue_t *q)
{
    if (!q)
    {
        return;
    }
    if (q->buf)
    {
        mem_free(q->allocator, q->buf, q->cap * q->elem_size);
    }
    allocator_t al = q->allocator;
    mem_free(al, q, sizeof(queue_t));
}

/* ---- iter -------------------------------------------------------------- */

typedef struct
{
    const char *buf;
    size_t head;
    size_t cap;
    size_t remaining;
    size_t elem_size;
} queue_iter_state_t;

static bool queue_iter_next(iter_t *it, void *out)
{
    queue_iter_state_t *s = it->state;
    if (s->remaining == 0 || s->cap == 0)
    {
        return false;
    }
    memcpy(out, s->buf + s->head * s->elem_size, s->elem_size);
    s->head = (s->head + 1) % s->cap;
    s->remaining--;
    return true;
}

static void queue_iter_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(queue_iter_state_t));
}

iter_t queue_iter(const queue_t *q)
{
    if (!q)
    {
        return (iter_t){0};
    }
    queue_iter_state_t *s =
        mem_alloc(q->allocator, sizeof *s, _Alignof(queue_iter_state_t));
    if (!s)
    {
        return (iter_t){0};
    }
    *s = (queue_iter_state_t){q->buf, q->head, q->cap, q->len, q->elem_size};
    return (iter_t){
        .next = queue_iter_next,
        .destroy = queue_iter_drop,
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
} queue_iter_rev_state_t;

static bool queue_iter_rev_next(iter_t *it, void *out)
{
    queue_iter_rev_state_t *s = it->state;
    if (s->remaining == 0 || s->cap == 0)
    {
        return false;
    }
    memcpy(out, s->buf + s->tail * s->elem_size, s->elem_size);
    s->tail = (s->tail + s->cap - 1) % s->cap;
    s->remaining--;
    return true;
}

static void queue_iter_rev_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(queue_iter_rev_state_t));
}

iter_t queue_iter_rev(const queue_t *q)
{
    if (!q)
    {
        return (iter_t){0};
    }
    queue_iter_rev_state_t *s =
        mem_alloc(q->allocator, sizeof *s, _Alignof(queue_iter_rev_state_t));
    if (!s)
    {
        return (iter_t){0};
    }
    size_t tail = (q->len > 0) ? (q->head + q->len - 1) % q->cap : 0;
    *s = (queue_iter_rev_state_t){q->buf, tail, q->cap, q->len, q->elem_size};
    return (iter_t){
        .next = queue_iter_rev_next,
        .destroy = queue_iter_rev_drop,
        .state = s,
        .elem_size = q->elem_size,
        .allocator = q->allocator};
}

seqc_status_t queue_front(const queue_t *q, void *out)
{
    if (!q)
    {
        return SEQC_INVALID;
    }
    return seqc_copy_out(queue_front_ptr(q), q->elem_size, out);
}

seqc_status_t queue_back(const queue_t *q, void *out)
{
    if (!q)
    {
        return SEQC_INVALID;
    }
    return seqc_copy_out(queue_back_ptr(q), q->elem_size, out);
}

static seqc_status_t add_push(void *q, const void *elem)
{
    return queue_push(q, elem);
}

seqc_status_t queue_extend(queue_t *q, iter_t it)
{
    if (!q)
    {
        iter_destroy(&it);
        return SEQC_INVALID;
    }
    return seqc_extend(q, add_push, q->elem_size, q->allocator, it);
}
