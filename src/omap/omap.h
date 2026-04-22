#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* Ordered map backed by an AVL tree.
 * Keys are kept sorted by compare_fn; all operations are O(log n).
 * compare_fn: same signature as iter_sort — negative / zero / positive. */

typedef struct OMNode OMNode;
struct OMNode {
  OMNode *left;
  OMNode *right;
  int height;
  /* key data then value data stored inline after the header */
};

/* Pointer-pair yielded by omap_iter — points directly into the live node.
 * Do not modify the tree while iterating. */
typedef struct {
  void *key;
  void *value;
} OMapEntry;

typedef struct {
  OMNode *root;
  size_t len;
  size_t key_size;
  size_t val_size;
  compare_fn cmp;
  Allocator allocator;
} OMap;

OMap omap_create(size_t key_size, size_t val_size, compare_fn cmp,
                 Allocator allocator);

/* Insert or update.  Returns true if a new key was inserted, false if updated.
 */
bool omap_set(OMap *m, const void *key, const void *value);

/* Returns pointer to the stored value, or NULL if not found. */
void *omap_get(const OMap *m, const void *key);

bool omap_contains(const OMap *m, const void *key);
bool omap_remove(OMap *m, const void *key); /* true=removed false=not found */
void *omap_min_key(const OMap *m);          /* NULL if empty         */
void *omap_max_key(const OMap *m);          /* NULL if empty         */
/* Return the full entry (key+value pointers) at the min or max key.
 * Returns an entry with both fields NULL if the map is empty. */
OMapEntry omap_min_entry(const OMap *m);
OMapEntry omap_max_entry(const OMap *m);
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
