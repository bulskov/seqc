#include "slice.h"

void *slice_get(Slice s, size_t i)
{
    if (s.ptr == NULL || s.elem_size == 0 || i >= s.len)
    {
        return NULL;
    }
    return (char *)s.ptr + i * s.elem_size;
}

void *slice_find(Slice s, bool (*pred)(const void *elem, void *ctx), void *ctx)
{
    if (s.ptr == NULL || s.elem_size == 0 || !pred)
        return NULL;
    for (size_t i = 0; i < s.len; i++)
    {
        void *elem = (char *)s.ptr + i * s.elem_size;
        if (pred(elem, ctx))
            return elem;
    }
    return NULL;
}

bool slice_contains(
    Slice s, bool (*pred)(const void *elem, void *ctx), void *ctx)
{
    return slice_find(s, pred, ctx) != NULL;
}
