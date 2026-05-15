#include "seqc/vec.h"
#include "arena/allocator.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 16

struct Vec
{
    void *data;
    size_t len;
    size_t cap;
    size_t elem_size;
    allocator_t allocator;
};

Vec *vec_create(size_t elem_size, allocator_t allocator)
{
    if (elem_size == 0 || !allocator.vt)
    {
        return NULL;
    }
    Vec *v = mem_alloc(allocator, sizeof(Vec), _Alignof(Vec));
    if (!v)
        return NULL;
    *v = (Vec){.data = NULL,
               .len = 0,
               .cap = 0,
               .elem_size = elem_size,
               .allocator = allocator};
    return v;
}

Vec *vec_create_size(size_t elem_size, size_t capacity, allocator_t allocator)
{
    if (elem_size == 0 || capacity == 0 || !allocator.vt)
    {
        return NULL;
    }
    Vec *v = mem_alloc(allocator, sizeof(Vec), _Alignof(Vec));
    if (!v)
        return NULL;
    *v = (Vec){.data = mem_alloc(
                   allocator, capacity * elem_size, _Alignof(max_align_t)),
               .len = 0,
               .cap = capacity,
               .elem_size = elem_size,
               .allocator = allocator};
    if (!v->data)
    {
        mem_free(allocator, v, sizeof(Vec));
        return NULL;
    }
    return v;
}

size_t vec_len(const Vec *v)
{
    return v ? v->len : 0;
}

size_t vec_elem_size(const Vec *v)
{
    return v ? v->elem_size : 0;
}

size_t vec_cap(const Vec *v)
{
    return v ? v->cap : 0;
}

SeqcStatus vec_push(Vec *v, const void *elem)
{
    if (!v || !elem)
        return SEQC_INVALID;
    if (v->len == v->cap)
    {
        if (v->cap > SIZE_MAX / 2)
            return SEQC_OOM;
        size_t new_cap = v->cap == 0 ? INITIAL_CAP : v->cap * 2;
        void *new_data = mem_realloc(
            v->allocator,

            v->data,
            v->len * v->elem_size,
            new_cap * v->elem_size,
            _Alignof(max_align_t));
        if (!new_data)
            return SEQC_OOM;
        v->data = new_data;
        v->cap = new_cap;
    }
    memcpy((char *)v->data + v->len * v->elem_size, elem, v->elem_size);
    v->len++;
    return SEQC_OK;
}

void *vec_get(const Vec *v, size_t i)
{
    if (!v || i >= v->len)
    {
        return NULL;
    }
    return (char *)v->data + i * v->elem_size;
}

SeqcStatus vec_get_copy(const Vec *v, size_t i, void *out)
{
    if (!v || !out)
        return SEQC_INVALID;
    if (i >= v->len)
        return SEQC_NOT_FOUND;
    memcpy(out, (char *)v->data + i * v->elem_size, v->elem_size);
    return SEQC_OK;
}

Slice vec_as_slice(const Vec *v)
{
    if (!v)
    {
        return (Slice){0};
    }
    return (Slice){v->data, v->len, v->elem_size};
}

Iter vec_iter(const Vec *v)
{
    if (!v)
    {
        return (Iter){0};
    }
    return iter_from_slice(vec_as_slice(v), v->allocator);
}

Iter vec_iter_rev(const Vec *v)
{
    if (!v)
    {
        return (Iter){0};
    }
    return iter_from_slice_rev(vec_as_slice(v), v->allocator);
}

SeqcStatus vec_pop(Vec *v, void *out)
{
    if (!v || v->len == 0)
        return SEQC_NOT_FOUND;
    v->len--;
    if (out)
        memcpy(out, (char *)v->data + v->len * v->elem_size, v->elem_size);
    return SEQC_OK;
}

void vec_set(Vec *v, size_t i, const void *elem)
{
    if (!v || !elem || i >= v->len)
        return;
    memcpy((char *)v->data + i * v->elem_size, elem, v->elem_size);
}

SeqcStatus vec_reserve(Vec *v, size_t capacity)
{
    if (!v)
        return SEQC_INVALID;
    if (capacity <= v->cap)
        return SEQC_OK;
    void *new_data = mem_realloc(
        v->allocator,

        v->data,
        v->len * v->elem_size,
        capacity * v->elem_size,
        _Alignof(max_align_t));
    if (!new_data)
        return SEQC_OOM;
    v->data = new_data;
    v->cap = capacity;
    return SEQC_OK;
}

SeqcStatus vec_insert(Vec *v, size_t i, const void *elem)
{
    if (!v || !elem || i > v->len)
        return SEQC_INVALID;
    /* ensure space */
    if (v->len == v->cap)
    {
        size_t new_cap = v->cap == 0 ? INITIAL_CAP : v->cap * 2;
        SeqcStatus s = vec_reserve(v, new_cap);
        if (s != SEQC_OK)
            return s;
    }
    /* shift elements [i .. len-1] right by one */
    memmove(
        (char *)v->data + (i + 1) * v->elem_size,
        (char *)v->data + i * v->elem_size,
        (v->len - i) * v->elem_size);
    memcpy((char *)v->data + i * v->elem_size, elem, v->elem_size);
    v->len++;
    return SEQC_OK;
}

void vec_remove(Vec *v, size_t i)
{
    if (!v || i >= v->len)
        return;
    /* shift elements [i+1 .. len-1] left by one */
    memmove(
        (char *)v->data + i * v->elem_size,
        (char *)v->data + (i + 1) * v->elem_size,
        (v->len - i - 1) * v->elem_size);
    v->len--;
}

void vec_clear(Vec *v)
{
    if (v)
        v->len = 0;
}

void *vec_find(const Vec *v, pred_fn pred, void *ctx)
{
    if (!v || !pred)
        return NULL;
    for (size_t i = 0; i < v->len; i++)
    {
        void *elem = (char *)v->data + i * v->elem_size;
        if (pred(elem, ctx))
            return elem;
    }
    return NULL;
}

bool vec_contains(const Vec *v, pred_fn pred, void *ctx)
{
    return vec_find(v, pred, ctx) != NULL;
}

void vec_free(Vec *v)
{
    if (!v)
        return;
    if (v->data)
        mem_free(v->allocator, v->data, v->cap * v->elem_size);
    allocator_t al = v->allocator;
    mem_free(al, v, sizeof(Vec));
}

void vec_sort(Vec *v, compare_fn cmp)
{
    if (!v || !cmp || v->len < 2)
        return;
    qsort(v->data, v->len, v->elem_size, cmp);
}

SeqcStatus vec_extend(Vec *v, Iter it)
{
    if (!v)
    {
        iter_drop(&it);
        return SEQC_INVALID;
    }
    void *elem = mem_alloc(v->allocator, v->elem_size, _Alignof(max_align_t));
    if (!elem)
    {
        iter_drop(&it);
        return SEQC_OOM;
    }
    SeqcStatus st = SEQC_OK;
    while (it.next(&it, elem))
    {
        st = vec_push(v, elem);
        if (st != SEQC_OK)
            break;
    }
    iter_drop(&it);
    mem_free(v->allocator, elem, v->elem_size);
    return st;
}
