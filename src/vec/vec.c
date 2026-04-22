#include "vec.h"
#include "arena/arena.h"

#include <string.h>

#define INITIAL_CAP 16

struct Vec
{
    void *data;
    size_t len;
    size_t cap;
    size_t elem_size;
    Allocator allocator;
};

Vec *vec_create(size_t elem_size, Allocator allocator)
{
    if (elem_size == 0 || !allocator.alloc || !allocator.realloc)
    {
        return NULL;
    }
    Vec *v = allocator.alloc(allocator.ctx, sizeof(Vec), _Alignof(Vec));
    if (!v)
        return NULL;
    *v = (Vec){
        .data = NULL,
        .len = 0,
        .cap = 0,
        .elem_size = elem_size,
        .allocator = allocator};
    return v;
}

Vec *vec_create_size(size_t elem_size, size_t capacity, Allocator allocator)
{
    if (elem_size == 0 || capacity == 0 || !allocator.alloc
        || !allocator.realloc)
    {
        return NULL;
    }
    Vec *v = allocator.alloc(allocator.ctx, sizeof(Vec), _Alignof(Vec));
    if (!v)
        return NULL;
    *v = (Vec){
        .data = allocator.alloc(
            allocator.ctx, capacity * elem_size, _Alignof(max_align_t)),
        .len = 0,
        .cap = capacity,
        .elem_size = elem_size,
        .allocator = allocator};
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

void vec_push(Vec *v, const void *elem)
{
    if (!v || !elem)
    {
        return;
    }
    if (v->len == v->cap)
    {
        v->cap = v->cap == 0 ? INITIAL_CAP : v->cap * 2;
        v->data = v->allocator.realloc(
            v->allocator.ctx,
            v->data,
            v->len * v->elem_size,
            v->cap * v->elem_size,
            _Alignof(max_align_t));
    }
    memcpy((char *)v->data + v->len * v->elem_size, elem, v->elem_size);
    v->len++;
}

void *vec_get(const Vec *v, size_t i)
{
    if (!v || i >= v->len)
    {
        return NULL;
    }
    return (char *)v->data + i * v->elem_size;
}

bool vec_get_copy(const Vec *v, size_t i, void *out)
{
    if (!v || i >= v->len || !out)
        return false;
    memcpy(out, (char *)v->data + i * v->elem_size, v->elem_size);
    return true;
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

bool vec_pop(Vec *v, void *out)
{
    if (!v || v->len == 0)
        return false;
    v->len--;
    if (out)
        memcpy(out, (char *)v->data + v->len * v->elem_size, v->elem_size);
    return true;
}

void vec_set(Vec *v, size_t i, const void *elem)
{
    if (!v || !elem || i >= v->len)
        return;
    memcpy((char *)v->data + i * v->elem_size, elem, v->elem_size);
}

void vec_reserve(Vec *v, size_t capacity)
{
    if (!v || capacity <= v->cap)
        return;
    v->data = v->allocator.realloc(
        v->allocator.ctx,
        v->data,
        v->len * v->elem_size,
        capacity * v->elem_size,
        _Alignof(max_align_t));
    v->cap = capacity;
}

void vec_insert(Vec *v, size_t i, const void *elem)
{
    if (!v || !elem || i > v->len)
        return;
    /* ensure space */
    if (v->len == v->cap)
    {
        size_t new_cap = v->cap == 0 ? INITIAL_CAP : v->cap * 2;
        vec_reserve(v, new_cap);
    }
    /* shift elements [i .. len-1] right by one */
    memmove(
        (char *)v->data + (i + 1) * v->elem_size,
        (char *)v->data + i * v->elem_size,
        (v->len - i) * v->elem_size);
    memcpy((char *)v->data + i * v->elem_size, elem, v->elem_size);
    v->len++;
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
    if (v->data && v->allocator.free)
        v->allocator.free(v->allocator.ctx, v->data);
    Allocator al = v->allocator;
    if (al.free)
        al.free(al.ctx, v);
}
