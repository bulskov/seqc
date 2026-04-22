#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* Ordered map backed by an AVL tree.
 * Keys are kept sorted by compare_fn; all operations are O(log n).
 * compare_fn: same signature as iter_sort — negative / zero / positive. */

typedef struct OMNode OMNode;
typedef struct OMap OMap;

/* Pointer-pair yielded by omap_iter — points directly into the live node.
 * Do not modify the tree while iterating. */
typedef struct
{
    void *key;
    void *value;
} OMapEntry;

OMap *omap_create(
    size_t key_size, size_t val_size, compare_fn cmp, Allocator allocator);

/* Insert or update.  Returns true if a new key was inserted, false if updated.
 */
bool omap_set(OMap *m, const void *key, const void *value);

/* Copies the value for key into out (may be NULL to test for presence only).
 * Returns true if the key was found, false otherwise. */
bool omap_get(const OMap *m, const void *key, void *out);

bool omap_contains(const OMap *m, const void *key);
bool omap_remove(OMap *m, const void *key); /* true=removed false=not found */
/* Copy the min/max key into out (may be NULL). Returns false if empty. */
bool omap_min_key(const OMap *m, void *out);
bool omap_max_key(const OMap *m, void *out);
/* Copy the key and value at the min/max position into key_out and val_out
 * (either may be NULL). Returns false if the map is empty. */
bool omap_min_entry(const OMap *m, void *key_out, void *val_out);
bool omap_max_entry(const OMap *m, void *key_out, void *val_out);
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
