#pragma once

#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* Self-balancing AVL tree.
 * compare_fn: same type as iter_sort — negative / zero / positive.
 * The balance invariant |height(L) - height(R)| <= 1 is maintained after
 * every insert and remove via LL / RR / LR / RL rotations. */

typedef struct AVLNode AVLNode;
struct AVLNode {
  AVLNode *left;
  AVLNode *right;
  int height; /* 1-based; 0 used as sentinel for NULL */
  /* element data stored inline after the header */
};

typedef struct {
  AVLNode *root;
  size_t len;
  size_t elem_size;
  compare_fn cmp;
  Allocator allocator;
} AVLTree;

AVLTree avl_create(size_t elem_size, compare_fn cmp, Allocator allocator);
bool avl_insert(AVLTree *t,
                const void *elem); /* true=inserted false=duplicate */
bool avl_contains(const AVLTree *t, const void *elem);
bool avl_remove(AVLTree *t,
                const void *elem); /* true=removed  false=not found */
void *avl_min(const AVLTree *t);   /* NULL if empty          */
void *avl_max(const AVLTree *t);   /* NULL if empty          */
size_t avl_len(const AVLTree *t);
int avl_height(const AVLTree *t);    /* 0 if empty             */
Iter avl_iter(const AVLTree *t);     /* ascending, in-order    */
Iter avl_iter_rev(const AVLTree *t); /* descending, in-order   */
/* Ascending in-order, only elements where lo <= elem <= hi.
 * NULL lo/hi means unbounded on that side. */
Iter avl_iter_range(const AVLTree *t, const void *lo, const void *hi);
void avl_free(AVLTree *t);
