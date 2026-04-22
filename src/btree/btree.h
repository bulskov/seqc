#pragma once

#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* Unbalanced binary search tree.
 * compare_fn is the same type used by iter_sort:
 *   int cmp(const void *a, const void *b) — negative / zero / positive */

typedef struct BTreeNode BTreeNode;
struct BTreeNode {
  BTreeNode *left;
  BTreeNode *right;
  /* element data stored inline after the header */
};

typedef struct {
  BTreeNode *root;
  size_t len;
  size_t elem_size;
  compare_fn cmp;
  Allocator allocator;
} BTree;

BTree btree_create(size_t elem_size, compare_fn cmp, Allocator allocator);
int btree_insert(BTree *t, const void *elem); /* 1=inserted 0=duplicate */
int btree_contains(const BTree *t, const void *elem);
int btree_remove(BTree *t, const void *elem); /* 1=removed  0=not found */
void *btree_min(const BTree *t);              /* NULL if empty          */
void *btree_max(const BTree *t);              /* NULL if empty          */
size_t btree_len(const BTree *t);
Iter btree_iter(const BTree *t);     /* ascending, in-order    */
Iter btree_iter_rev(const BTree *t); /* descending, in-order   */
void btree_free(BTree *t);
