#pragma once

#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* Self-balancing AVL tree.
 * compare_fn: same type as iter_sort — negative / zero / positive.
 * The balance invariant |height(L) - height(R)| <= 1 is maintained after
 * every insert and remove via LL / RR / LR / RL rotations. */

typedef struct AVLNode AVLNode;
typedef struct AVLTree AVLTree;

AVLTree *avl_create(size_t elem_size, compare_fn cmp, allocator_t allocator);
/* SEQC_OK=inserted, SEQC_DUPLICATE=already present, SEQC_OOM=alloc failure */
SeqcStatus avl_insert(AVLTree *t, const void *elem);
bool avl_contains(const AVLTree *t, const void *elem);
/* SEQC_OK=removed, SEQC_NOT_FOUND=absent */
SeqcStatus avl_remove(AVLTree *t, const void *elem);
void *avl_min(const AVLTree *t); /* NULL if empty          */
void *avl_max(const AVLTree *t); /* NULL if empty          */
size_t avl_len(const AVLTree *t);
int avl_height(const AVLTree *t);    /* 0 if empty             */
Iter avl_iter(const AVLTree *t);     /* ascending, in-order    */
Iter avl_iter_rev(const AVLTree *t); /* descending, in-order   */
/* Ascending in-order, only elements where lo <= elem <= hi.
 * NULL lo/hi means unbounded on that side. */
Iter avl_iter_range(const AVLTree *t, const void *lo, const void *hi);
void avl_free(AVLTree *t);
void avl_clear(AVLTree *t);
