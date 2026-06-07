#pragma once

#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* Self-balancing AVL tree.
 * compare_fn: same type as iter_sort — negative / zero / positive.
 * The balance invariant |height(L) - height(R)| <= 1 is maintained after
 * every insert and remove via LL / RR / LR / RL rotations. */

typedef struct avl_node_t avl_node_t;
typedef struct avl_t avl_t;

avl_t *avl_create(size_t elem_size, compare_fn cmp, allocator_t allocator);
/* SEQC_OK=inserted, SEQC_DUPLICATE=already present, SEQC_OOM=alloc failure */
seqc_status_t avl_insert(avl_t *t, const void *elem);
bool avl_contains(const avl_t *t, const void *elem);
/* SEQC_OK=removed, SEQC_NOT_FOUND=absent */
seqc_status_t avl_remove(avl_t *t, const void *elem);
void *avl_min(const avl_t *t); /* NULL if empty          */
void *avl_max(const avl_t *t); /* NULL if empty          */
size_t avl_len(const avl_t *t);
int avl_height(const avl_t *t);    /* 0 if empty             */
iter_t avl_iter(const avl_t *t);     /* ascending, in-order    */
iter_t avl_iter_rev(const avl_t *t); /* descending, in-order   */
/* Ascending in-order, only elements where lo <= elem <= hi.
 * NULL lo/hi means unbounded on that side. */
iter_t avl_iter_range(const avl_t *t, const void *lo, const void *hi);
void avl_free(avl_t *t);
void avl_clear(avl_t *t);
