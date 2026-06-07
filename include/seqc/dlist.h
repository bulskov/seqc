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
void *dlist_front(const dlist_t *l); /* pointer to head data; NULL if empty */
void *dlist_back(const dlist_t *l);  /* pointer to tail data; NULL if empty */
bool dlist_is_empty(const dlist_t *l);
size_t dlist_len(const dlist_t *l);
iter_t dlist_iter(const dlist_t *l);     /* front→back */
iter_t dlist_iter_rev(const dlist_t *l); /* back→front */
void dlist_clear(dlist_t *l);          /* remove all nodes */
void dlist_free(dlist_t *l);
