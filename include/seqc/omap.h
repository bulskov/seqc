#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* Ordered map backed by an AVL tree.
 * Keys are kept sorted by compare_fn; all operations are O(log n).
 * compare_fn: same signature as iter_sort — negative / zero / positive. */

typedef struct omap_node_t omap_node_t;
typedef struct omap_t omap_t;

/* Pointer-pair yielded by omap_iter \u2014 see map_entry_t in iter/iter.h.
 * Do not modify the tree while iterating. */
typedef map_entry_t omap_entry_t;

omap_t *omap_create(
    size_t key_size, size_t val_size, compare_fn cmp, allocator_t allocator);

/* Insert or update.  Returns SEQC_OK on success, SEQC_OOM on alloc failure. */
seqc_status_t omap_set(omap_t *m, const void *key, const void *value);

/* Copies the value for key into out (may be NULL to test for presence only).
 * Returns SEQC_OK if found, SEQC_NOT_FOUND otherwise. */
seqc_status_t omap_get(const omap_t *m, const void *key, void *out);

bool omap_contains(const omap_t *m, const void *key);
/* SEQC_OK=removed, SEQC_NOT_FOUND=absent */
seqc_status_t omap_remove(omap_t *m, const void *key);
/* Copy the min/max key into out (may be NULL). Returns SEQC_NOT_FOUND if empty.
 */
seqc_status_t omap_min_key(const omap_t *m, void *out);
seqc_status_t omap_max_key(const omap_t *m, void *out);
/* Copy the key and value at the min/max position into key_out and val_out
 * (either may be NULL). Returns SEQC_NOT_FOUND if the map is empty. */
seqc_status_t omap_min_entry(const omap_t *m, void *key_out, void *val_out);
seqc_status_t omap_max_entry(const omap_t *m, void *key_out, void *val_out);
size_t omap_len(const omap_t *m);
int omap_height(const omap_t *m); /* 0 if empty            */

/* Yields omap_entry_t in ascending key order */
iter_t omap_iter(const omap_t *m);
/* Yields omap_entry_t in descending key order */
iter_t omap_iter_rev(const omap_t *m);
/* Yields omap_entry_t in ascending key order where lo_key <= key <= hi_key.
 * NULL lo_key/hi_key means unbounded on that side. */
iter_t omap_iter_range(const omap_t *m, const void *lo_key, const void *hi_key);

void omap_free(omap_t *m);
void omap_clear(omap_t *m);
