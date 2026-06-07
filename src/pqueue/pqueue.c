#include "seqc/pqueue.h"

#include <string.h>

struct pqueue_t
{
    vec_t *data;
    compare_fn cmp;
    allocator_t allocator;
};

pqueue_t *pqueue_create(size_t elem_size, compare_fn cmp, allocator_t allocator)
{
    pqueue_t *q = mem_alloc(allocator, sizeof(pqueue_t), _Alignof(pqueue_t));
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

static void sift_up(pqueue_t *q, size_t i)
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

static void sift_down(pqueue_t *q, size_t i)
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

pqueue_t *pqueue_build_from_vec(
    const vec_t *v, compare_fn cmp, allocator_t allocator)
{
    slice_t s = vec_as_slice(v);
    pqueue_t *q = pqueue_create(s.elem_size, cmp, allocator);
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

seqc_status_t pqueue_push(pqueue_t *q, const void *elem)
{
    if (!q || !elem)
        return SEQC_INVALID;
    seqc_status_t s = vec_push(q->data, elem);
    if (s != SEQC_OK)
        return s;
    sift_up(q, vec_len(q->data) - 1);
    return SEQC_OK;
}

seqc_status_t pqueue_pop(pqueue_t *q, void *out)
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

seqc_status_t pqueue_peek(const pqueue_t *q, void *out)
{
    if (!q || vec_len(q->data) == 0)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, vec_get(q->data, 0), vec_elem_size(q->data));
    return SEQC_OK;
}

size_t pqueue_len(const pqueue_t *q)
{
    return q ? vec_len(q->data) : 0;
}
bool pqueue_is_empty(const pqueue_t *q)
{
    return !q || vec_len(q->data) == 0;
}

void pqueue_clear(pqueue_t *q)
{
    if (q)
        vec_clear(q->data);
}

void pqueue_free(pqueue_t *q)
{
    if (!q)
        return;
    vec_free(q->data);
    allocator_t al = q->allocator;
    mem_free(al, q, sizeof(pqueue_t));
}

iter_t pqueue_iter(const pqueue_t *q)
{
    if (!q)
        return (iter_t){0};
    return vec_iter(q->data);
}

iter_t pqueue_iter_rev(const pqueue_t *q)
{
    if (!q)
        return (iter_t){0};
    return vec_iter_rev(q->data);
}

slice_t pqueue_drain(pqueue_t *q, allocator_t allocator)
{
    size_t n = pqueue_len(q);
    size_t elem_size = vec_elem_size(q->data);
    if (n == 0)
        return (slice_t){NULL, 0, elem_size};
    char *buf = mem_alloc(allocator, n * elem_size, _Alignof(max_align_t));
    if (!buf)
    {
        pqueue_clear(q);
        return (slice_t){NULL, 0, elem_size};
    }
    for (size_t i = 0; i < n; i++)
        pqueue_pop(q, buf + i * elem_size);
    return (slice_t){buf, n, elem_size};
}
