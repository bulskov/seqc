#pragma once

#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* Unbalanced binary search tree.
 * compare_fn is the same type used by iter_sort:
 *   int cmp(const void *a, const void *b) — negative / zero / positive */

typedef struct BTreeNode BTreeNode;
typedef struct BTree BTree;

BTree *btree_create(size_t elem_size, compare_fn cmp, Allocator allocator);
bool btree_insert(
    BTree *t, const void *elem); /* true=inserted false=duplicate */
bool btree_contains(const BTree *t, const void *elem);
bool btree_remove(
    BTree *t, const void *elem); /* true=removed  false=not found */
void *btree_min(const BTree *t); /* NULL if empty          */
void *btree_max(const BTree *t); /* NULL if empty          */
size_t btree_len(const BTree *t);
int btree_height(const BTree *t);    /* 0 if empty             */
Iter btree_iter(const BTree *t);     /* ascending, in-order    */
Iter btree_iter_rev(const BTree *t); /* descending, in-order   */
/* Ascending in-order, only elements where lo <= elem <= hi.
 * NULL lo/hi means unbounded on that side. */
Iter btree_iter_range(const BTree *t, const void *lo, const void *hi);
void btree_free(BTree *t);
void btree_clear(BTree *t);
