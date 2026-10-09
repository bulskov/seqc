#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* Doubly-linked list.  Element data is stored inline immediately after each
 * node header, aligned to max_align_t. */
typedef struct dlist_node_t dlist_node_t;

typedef struct dlist_t dlist_t;

dlist_t *dlist_create(size_t elem_size, allocator_t allocator);
seqc_status_t dlist_push_front(dlist_t *l, const void *elem);
seqc_status_t dlist_push_back(dlist_t *l, const void *elem);
seqc_status_t dlist_pop_front(dlist_t *l, void *out); /* out may be NULL */
seqc_status_t dlist_pop_back(dlist_t *l, void *out);  /* out may be NULL */
/* The first / last element.  Copies into out (may be NULL to test only), so it
 * is not affected by later changes.  SEQC_NOT_FOUND if empty. */
seqc_status_t dlist_front(const dlist_t *l, void *out);
seqc_status_t dlist_back(const dlist_t *l, void *out);
/* Pointers to the first / last element, NULL if empty.  Nodes never move:
 * valid until that element is removed or the list is cleared/destroyed. */
void *dlist_front_ptr(const dlist_t *l);
void *dlist_back_ptr(const dlist_t *l);
bool dlist_is_empty(const dlist_t *l);
size_t dlist_len(const dlist_t *l);
iter_t dlist_iter(const dlist_t *l);     /* front→back */
iter_t dlist_iter_rev(const dlist_t *l); /* back→front */
/* Push every element of it (consumed) at the back; stops at the first
 * error. */
seqc_status_t dlist_extend(dlist_t *l, iter_t it);
void dlist_clear(dlist_t *l); /* remove all nodes */
void dlist_destroy(dlist_t *l);
