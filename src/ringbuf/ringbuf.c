#include "seqc/ringbuf.h"

#include <assert.h>
#include <string.h>

#define RINGBUF_INITIAL_CAP 8

struct RingBuf
{
    void *data;
    size_t cap; /* always a power of 2, or 0 when uninitialised */
    size_t len;
    size_t head; /* index of first element */
    size_t elem_size;
    allocator_t allocator;
};

/* ---- internal helpers -------------------------------------------------- */

static inline size_t rb_slot(const RingBuf *r, size_t logical)
{
    return (r->head + logical) & (r->cap - 1);
}

static inline void *rb_ptr(const RingBuf *r, size_t slot)
{
    return (char *)r->data + slot * r->elem_size;
}

/* Grow to at least new_cap (rounded up to next power of 2). */
static SeqcStatus rb_grow(RingBuf *r)
{
    size_t new_cap = r->cap == 0 ? RINGBUF_INITIAL_CAP : r->cap * 2;
    void *nd = mem_alloc(r->allocator,
         new_cap * r->elem_size, _Alignof(max_align_t));
    if (!nd)
        return SEQC_OOM;

    /* Linearise: copy front..end then wrap..head (if any) */
    if (r->len > 0)
    {
        size_t tail = r->head + r->len; /* logical */
        if (tail <= r->cap)
        {
            /* contiguous */
            memcpy(nd, rb_ptr(r, r->head), r->len * r->elem_size);
        }
        else
        {
            /* wrapped: copy from head to end, then from 0 to wrap point */
            size_t first = r->cap - r->head;
            memcpy(nd, rb_ptr(r, r->head), first * r->elem_size);
            memcpy(
                (char *)nd + first * r->elem_size,
                r->data,
                (r->len - first) * r->elem_size);
        }
    }

    if (r->data)
        mem_free(r->allocator, r->data, r->cap * r->elem_size);
    r->data = nd;
    r->cap = new_cap;
    r->head = 0;
    return SEQC_OK;
}

/* ---- public API -------------------------------------------------------- */

RingBuf *ringbuf_create(size_t elem_size, allocator_t allocator)
{
    RingBuf *r =
        mem_alloc(allocator, sizeof(RingBuf), _Alignof(RingBuf));
    if (!r)
        return NULL;
    *r = (RingBuf){
        .data = NULL,
        .cap = 0,
        .len = 0,
        .head = 0,
        .elem_size = elem_size,
        .allocator = allocator,
    };
    return r;
}

SeqcStatus ringbuf_push_back(RingBuf *r, const void *elem)
{
    if (!r || !elem)
        return SEQC_INVALID;
    if (r->len == r->cap)
    {
        SeqcStatus st = rb_grow(r);
        if (st != SEQC_OK)
            return st;
    }
    size_t slot = rb_slot(r, r->len);
    memcpy(rb_ptr(r, slot), elem, r->elem_size);
    r->len++;
    return SEQC_OK;
}

SeqcStatus ringbuf_push_front(RingBuf *r, const void *elem)
{
    if (!r || !elem)
        return SEQC_INVALID;
    if (r->len == r->cap)
    {
        SeqcStatus st = rb_grow(r);
        if (st != SEQC_OK)
            return st;
    }
    /* move head back by one (wrapping) */
    r->head = (r->head + r->cap - 1) & (r->cap - 1);
    memcpy(rb_ptr(r, r->head), elem, r->elem_size);
    r->len++;
    return SEQC_OK;
}

SeqcStatus ringbuf_pop_front(RingBuf *r, void *out)
{
    if (!r || r->len == 0)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, rb_ptr(r, r->head), r->elem_size);
    r->head = (r->head + 1) & (r->cap - 1);
    r->len--;
    return SEQC_OK;
}

SeqcStatus ringbuf_pop_back(RingBuf *r, void *out)
{
    if (!r || r->len == 0)
        return SEQC_NOT_FOUND;
    size_t slot = rb_slot(r, r->len - 1);
    if (out)
        memcpy(out, rb_ptr(r, slot), r->elem_size);
    r->len--;
    return SEQC_OK;
}

SeqcStatus ringbuf_at(const RingBuf *r, size_t i, void *out)
{
    if (!r || i >= r->len)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, rb_ptr(r, rb_slot(r, i)), r->elem_size);
    return SEQC_OK;
}

size_t ringbuf_len(const RingBuf *r)
{
    return r ? r->len : 0;
}

size_t ringbuf_cap(const RingBuf *r)
{
    return r ? r->cap : 0;
}

bool ringbuf_is_empty(const RingBuf *r)
{
    return !r || r->len == 0;
}

void ringbuf_clear(RingBuf *r)
{
    if (!r)
        return;
    r->len = 0;
    r->head = 0;
}

void ringbuf_free(RingBuf *r)
{
    if (!r)
        return;
    if (r->data)
        mem_free(r->allocator, r->data, r->cap * r->elem_size);
    allocator_t al = r->allocator;
    mem_free(al, r, sizeof(RingBuf));
}

/* ---- iter -------------------------------------------------------------- */

typedef struct
{
    const RingBuf *r;
    size_t pos; /* logical index; 0 = front */
} RingBufIterState;

static bool ringbuf_iter_next(Iter *it, void *out)
{
    RingBufIterState *st = it->state;
    if (st->pos >= st->r->len)
        return false;
    memcpy(out, rb_ptr(st->r, rb_slot(st->r, st->pos)), st->r->elem_size);
    st->pos++;
    return true;
}

static bool ringbuf_iter_rev_next(Iter *it, void *out)
{
    RingBufIterState *st = it->state;
    if (st->pos == 0)
        return false;
    st->pos--;
    memcpy(out, rb_ptr(st->r, rb_slot(st->r, st->pos)), st->r->elem_size);
    return true;
}

static void ringbuf_iter_drop(Iter *it)
{
    mem_free(it->allocator, it->state, sizeof(RingBufIterState));
}

Iter ringbuf_iter(const RingBuf *r)
{
    if (!r)
        return (Iter){0};
    RingBufIterState *st = mem_alloc(r->allocator,
         sizeof *st, _Alignof(RingBufIterState));
    if (!st)
        return (Iter){0};
    *st = (RingBufIterState){r, 0};
    return (Iter){
        .next = ringbuf_iter_next,
        .drop = ringbuf_iter_drop,
        .state = st,
        .elem_size = r->elem_size,
        .allocator = r->allocator,
    };
}

Iter ringbuf_iter_rev(const RingBuf *r)
{
    if (!r)
        return (Iter){0};
    RingBufIterState *st = mem_alloc(r->allocator,
         sizeof *st, _Alignof(RingBufIterState));
    if (!st)
        return (Iter){0};
    *st = (RingBufIterState){r, r->len}; /* starts past the last element */
    return (Iter){
        .next = ringbuf_iter_rev_next,
        .drop = ringbuf_iter_drop,
        .state = st,
        .elem_size = r->elem_size,
        .allocator = r->allocator,
    };
}
