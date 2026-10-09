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
seqc_status_t avl_add(avl_t *t, const void *elem);
bool avl_contains(const avl_t *t, const void *elem);
/* SEQC_OK=removed, SEQC_NOT_FOUND=absent */
seqc_status_t avl_remove(avl_t *t, const void *elem);
/* The smallest / largest element.  Copies into out (may be NULL to test only),
 * so it is not affected by later changes.  SEQC_NOT_FOUND if empty. */
seqc_status_t avl_min(const avl_t *t, void *out);
seqc_status_t avl_max(const avl_t *t, void *out);
/* Pointers to the smallest / largest element, NULL if empty — valid until
 * that element is removed (removing others may move it), or the tree is
 * cleared/destroyed. */
void *avl_min_ptr(const avl_t *t);
void *avl_max_ptr(const avl_t *t);
size_t avl_len(const avl_t *t);
bool avl_is_empty(const avl_t *t);
int avl_height(const avl_t *t);      /* 0 if empty             */
iter_t avl_iter(const avl_t *t);     /* ascending, in-order    */
iter_t avl_iter_rev(const avl_t *t); /* descending, in-order   */
/* Ascending in-order, only elements where lo <= elem <= hi.
 * NULL lo/hi means unbounded on that side. */
iter_t avl_iter_range(const avl_t *t, const void *lo, const void *hi);
void avl_destroy(avl_t *t);
/* Add every element of it (consumed); duplicates are skipped, the first
 * other error stops it. */
seqc_status_t avl_extend(avl_t *t, iter_t it);
void avl_clear(avl_t *t);
