#pragma once

/* Internal: patterns shared by the collections.  Not installed.
 *
 * Every element accessor comes as a pair (see docs/naming.md):
 *   X_..._ptr  returns a pointer into the collection, NULL if absent —
 *              valid until the next change to the collection;
 *   X_...      copies the element into out and returns a status — not
 *              affected by later changes.
 * The pointer version does the lookup; the copy version is
 * seqc_copy_out(X_..._ptr(...)), so the two can never disagree. */

#include <string.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* The copy half of an accessor pair.  elem NULL -> SEQC_NOT_FOUND.  out
 * may be NULL to test for presence only. */
static inline seqc_status_t seqc_copy_out(
    const void *elem, size_t elem_size, void *out)
{
    if (!elem)
    {
        return SEQC_NOT_FOUND;
    }
    if (out)
    {
        memcpy(out, elem, elem_size);
    }
    return SEQC_OK;
}

/* Adds one element to a collection: X_push, X_push_back, X_add, ... */
typedef seqc_status_t (*seqc_add_fn)(void *coll, const void *elem);

/* The body of every X_extend: drain it into coll with add(coll, elem),
 * through a temporary element buffer from alloc.  Consumes the iterator on
 * every path.  SEQC_DUPLICATE from add is not an error (sets skip
 * duplicates); any other error stops the drain and is returned. */
seqc_status_t seqc_extend(
    void *coll,
    seqc_add_fn add,
    size_t elem_size,
    allocator_t alloc,
    iter_t it);
