#include "seqc/slice.h"
#include "collection.h"

void *slice_get_ptr(slice_t s, size_t i)
{
    if (s.ptr == NULL || s.elem_size == 0 || i >= s.len)
    {
        return NULL;
    }
    return (char *)s.ptr + i * s.elem_size;
}

void *slice_find(
    slice_t s, bool (*pred)(const void *elem, void *ctx), void *ctx)
{
    if (s.ptr == NULL || s.elem_size == 0 || !pred)
    {
        return NULL;
    }
    for (size_t i = 0; i < s.len; i++)
    {
        void *elem = (char *)s.ptr + i * s.elem_size;
        if (pred(elem, ctx))
        {
            return elem;
        }
    }
    return NULL;
}

bool slice_any(slice_t s, bool (*pred)(const void *elem, void *ctx), void *ctx)
{
    return slice_find(s, pred, ctx) != NULL;
}

seqc_status_t slice_get(slice_t s, size_t i, void *out)
{
    return seqc_copy_out(slice_get_ptr(s, i), s.elem_size, out);
}
