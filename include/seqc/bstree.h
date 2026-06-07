#pragma once

#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* Unbalanced binary search tree.
 * compare_fn is the same type used by iter_sort:
 *   int cmp(const void *a, const void *b) — negative / zero / positive */

typedef struct bstree_node_t bstree_node_t;
typedef struct bstree_t bstree_t;

bstree_t *bstree_create(size_t elem_size, compare_fn cmp, allocator_t allocator);
/* SEQC_OK=inserted, SEQC_DUPLICATE=already present, SEQC_OOM=alloc failure */
seqc_status_t bstree_insert(bstree_t *t, const void *elem);
bool bstree_contains(const bstree_t *t, const void *elem);
/* SEQC_OK=removed, SEQC_NOT_FOUND=absent */
seqc_status_t bstree_remove(bstree_t *t, const void *elem);
void *bstree_min(const bstree_t *t); /* NULL if empty          */
void *bstree_max(const bstree_t *t); /* NULL if empty          */
size_t bstree_len(const bstree_t *t);
int bstree_height(const bstree_t *t);    /* 0 if empty             */
iter_t bstree_iter(const bstree_t *t);     /* ascending, in-order    */
iter_t bstree_iter_rev(const bstree_t *t); /* descending, in-order   */
/* Ascending in-order, only elements where lo <= elem <= hi.
 * NULL lo/hi means unbounded on that side. */
iter_t bstree_iter_range(const bstree_t *t, const void *lo, const void *hi);
void bstree_free(bstree_t *t);
void bstree_clear(bstree_t *t);
