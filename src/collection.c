#include "collection.h"

#include "max_align.h"

seqc_status_t seqc_extend(
    void *coll, seqc_add_fn add, size_t elem_size, allocator_t alloc, iter_t it)
{
    if (!coll)
    {
        iter_destroy(&it);
        return SEQC_INVALID;
    }
    void *elem = mem_alloc(alloc, elem_size, SEQC_MAX_ALIGN);
    if (!elem)
    {
        iter_destroy(&it);
        return SEQC_OOM;
    }
    seqc_status_t st = SEQC_OK;
    while (it.next(&it, elem))
    {
        seqc_status_t added = add(coll, elem);
        if (added != SEQC_OK && added != SEQC_DUPLICATE)
        {
            st = added;
            break;
        }
    }
    iter_destroy(&it);
    mem_free(alloc, elem, elem_size);
    return st;
}
