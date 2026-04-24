#pragma once

#include <stddef.h>

#include "seqc/arena.h"
#include "seqc/iter.h"

/* Unbalanced binary search tree.
 * compare_fn is the same type used by iter_sort:
 *   int cmp(const void *a, const void *b) — negative / zero / positive */

typedef struct BSTreeNode BSTreeNode;
typedef struct BSTree BSTree;

BSTree *bstree_create(size_t elem_size, compare_fn cmp, Allocator allocator);
/* SEQC_OK=inserted, SEQC_DUPLICATE=already present, SEQC_OOM=alloc failure */
SeqcStatus bstree_insert(BSTree *t, const void *elem);
bool bstree_contains(const BSTree *t, const void *elem);
/* SEQC_OK=removed, SEQC_NOT_FOUND=absent */
SeqcStatus bstree_remove(BSTree *t, const void *elem);
void *bstree_min(const BSTree *t); /* NULL if empty          */
void *bstree_max(const BSTree *t); /* NULL if empty          */
size_t bstree_len(const BSTree *t);
int bstree_height(const BSTree *t);    /* 0 if empty             */
Iter bstree_iter(const BSTree *t);     /* ascending, in-order    */
Iter bstree_iter_rev(const BSTree *t); /* descending, in-order   */
/* Ascending in-order, only elements where lo <= elem <= hi.
 * NULL lo/hi means unbounded on that side. */
Iter bstree_iter_range(const BSTree *t, const void *lo, const void *hi);
void bstree_free(BSTree *t);
void bstree_clear(BSTree *t);
