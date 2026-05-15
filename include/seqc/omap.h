#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* Ordered map backed by an AVL tree.
 * Keys are kept sorted by compare_fn; all operations are O(log n).
 * compare_fn: same signature as iter_sort — negative / zero / positive. */

typedef struct OMNode OMNode;
typedef struct OMap OMap;

/* Pointer-pair yielded by omap_iter \u2014 see MapEntry in iter/iter.h.
 * Do not modify the tree while iterating. */
typedef MapEntry OMapEntry;

OMap *omap_create(
    size_t key_size, size_t val_size, compare_fn cmp, allocator_t allocator);

/* Insert or update.  Returns SEQC_OK on success, SEQC_OOM on alloc failure. */
SeqcStatus omap_set(OMap *m, const void *key, const void *value);

/* Copies the value for key into out (may be NULL to test for presence only).
 * Returns SEQC_OK if found, SEQC_NOT_FOUND otherwise. */
SeqcStatus omap_get(const OMap *m, const void *key, void *out);

bool omap_contains(const OMap *m, const void *key);
/* SEQC_OK=removed, SEQC_NOT_FOUND=absent */
SeqcStatus omap_remove(OMap *m, const void *key);
/* Copy the min/max key into out (may be NULL). Returns SEQC_NOT_FOUND if empty.
 */
SeqcStatus omap_min_key(const OMap *m, void *out);
SeqcStatus omap_max_key(const OMap *m, void *out);
/* Copy the key and value at the min/max position into key_out and val_out
 * (either may be NULL). Returns SEQC_NOT_FOUND if the map is empty. */
SeqcStatus omap_min_entry(const OMap *m, void *key_out, void *val_out);
SeqcStatus omap_max_entry(const OMap *m, void *key_out, void *val_out);
size_t omap_len(const OMap *m);
int omap_height(const OMap *m); /* 0 if empty            */

/* Yields OMapEntry in ascending key order */
Iter omap_iter(const OMap *m);
/* Yields OMapEntry in descending key order */
Iter omap_iter_rev(const OMap *m);
/* Yields OMapEntry in ascending key order where lo_key <= key <= hi_key.
 * NULL lo_key/hi_key means unbounded on that side. */
Iter omap_iter_range(const OMap *m, const void *lo_key, const void *hi_key);

void omap_free(OMap *m);
void omap_clear(OMap *m);
