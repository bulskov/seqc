#include "seqc/pqueue.h"

#include <string.h>

struct PQueue
{
    Vec *data;
    compare_fn cmp;
    Allocator allocator;
};

PQueue *pqueue_create(size_t elem_size, compare_fn cmp, Allocator allocator)
{
    PQueue *q =
        allocator.alloc(allocator.ctx, sizeof(PQueue), _Alignof(PQueue));
    if (!q)
        return NULL;
    q->data = vec_create(elem_size, allocator);
    q->cmp = cmp;
    q->allocator = allocator;
    return q;
}

/* ---- helpers ----------------------------------------------------------- */

static void swap_elems(void *a, void *b, size_t size)
{
    char *ca = a, *cb = b;
    while (size--)
    {
        char tmp = *ca;
        *ca++ = *cb;
        *cb++ = tmp;
    }
}

static void sift_up(PQueue *q, size_t i)
{
    while (i > 0)
    {
        size_t parent = (i - 1) / 2;
        if (q->cmp(vec_get(q->data, i), vec_get(q->data, parent)) < 0)
        {
            swap_elems(
                vec_get(q->data, i),
                vec_get(q->data, parent),
                vec_elem_size(q->data));
            i = parent;
        }
        else
        {
            break;
        }
    }
}

static void sift_down(PQueue *q, size_t i)
{
    size_t n = vec_len(q->data);
    while (1)
    {
        size_t smallest = i;
        size_t left = 2 * i + 1;
        size_t right = 2 * i + 2;
        if (left < n
            && q->cmp(vec_get(q->data, left), vec_get(q->data, smallest)) < 0)
            smallest = left;
        if (right < n
            && q->cmp(vec_get(q->data, right), vec_get(q->data, smallest)) < 0)
            smallest = right;
        if (smallest == i)
            break;
        swap_elems(
            vec_get(q->data, i),
            vec_get(q->data, smallest),
            vec_elem_size(q->data));
        i = smallest;
    }
}

/* ---- public API -------------------------------------------------------- */

PQueue *pqueue_build_from_vec(const Vec *v, compare_fn cmp, Allocator allocator)
{
    Slice s = vec_as_slice(v);
    PQueue *q = pqueue_create(s.elem_size, cmp, allocator);
    if (!q)
        return NULL;
    for (size_t i = 0; i < s.len; i++)
        vec_push(q->data, (char *)s.ptr + i * s.elem_size);
    /* Floyd's heapify: sift down every non-leaf from bottom up, O(n) */
    if (s.len > 1)
    {
        size_t i = s.len / 2;
        do
        {
            i--;
            sift_down(q, i);
        } while (i > 0);
    }
    return q;
}

SeqcStatus pqueue_push(PQueue *q, const void *elem)
{
    if (!q || !elem)
        return SEQC_INVALID;
    SeqcStatus s = vec_push(q->data, elem);
    if (s != SEQC_OK)
        return s;
    sift_up(q, vec_len(q->data) - 1);
    return SEQC_OK;
}

SeqcStatus pqueue_pop(PQueue *q, void *out)
{
    if (!q || vec_len(q->data) == 0)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, vec_get(q->data, 0), vec_elem_size(q->data));
    size_t last = vec_len(q->data) - 1;
    if (last > 0)
        memcpy(
            vec_get(q->data, 0),
            vec_get(q->data, last),
            vec_elem_size(q->data));
    vec_pop(q->data, NULL);
    if (vec_len(q->data) > 0)
        sift_down(q, 0);
    return SEQC_OK;
}

SeqcStatus pqueue_peek(const PQueue *q, void *out)
{
    if (!q || vec_len(q->data) == 0)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, vec_get(q->data, 0), vec_elem_size(q->data));
    return SEQC_OK;
}

size_t pqueue_len(const PQueue *q)
{
    return q ? vec_len(q->data) : 0;
}
bool pqueue_is_empty(const PQueue *q)
{
    return !q || vec_len(q->data) == 0;
}

void pqueue_clear(PQueue *q)
{
    if (q)
        vec_clear(q->data);
}

void pqueue_free(PQueue *q)
{
    if (!q)
        return;
    vec_free(q->data);
    Allocator al = q->allocator;
    if (al.free)
        al.free(al.ctx, q);
}

Iter pqueue_iter(const PQueue *q)
{
    if (!q)
        return (Iter){0};
    return vec_iter(q->data);
}

Iter pqueue_iter_rev(const PQueue *q)
{
    if (!q)
        return (Iter){0};
    return vec_iter_rev(q->data);
}

Slice pqueue_drain(PQueue *q, Allocator allocator)
{
    size_t n = pqueue_len(q);
    size_t elem_size = vec_elem_size(q->data);
    if (n == 0)
        return (Slice){NULL, 0, elem_size};
    char *buf =
        allocator.alloc(allocator.ctx, n * elem_size, _Alignof(max_align_t));
    if (!buf)
    {
        pqueue_clear(q);
        return (Slice){NULL, 0, elem_size};
    }
    for (size_t i = 0; i < n; i++)
        pqueue_pop(q, buf + i * elem_size);
    return (Slice){buf, n, elem_size};
}
