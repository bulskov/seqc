#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "seqc/arena.h"
#include "seqc/iter.h"

/* Doubly-linked list.  Element data is stored inline immediately after each
 * node header, aligned to max_align_t. */
typedef struct DListNode DListNode;

typedef struct DList DList;

DList *dlist_create(size_t elem_size, Allocator allocator);
SeqcStatus dlist_push_front(DList *l, const void *elem);
SeqcStatus dlist_push_back(DList *l, const void *elem);
SeqcStatus dlist_pop_front(DList *l, void *out); /* out may be NULL */
SeqcStatus dlist_pop_back(DList *l, void *out);  /* out may be NULL */
void *dlist_front(const DList *l); /* pointer to head data; NULL if empty */
void *dlist_back(const DList *l);  /* pointer to tail data; NULL if empty */
bool dlist_is_empty(const DList *l);
size_t dlist_len(const DList *l);
Iter dlist_iter(const DList *l);     /* front→back */
Iter dlist_iter_rev(const DList *l); /* back→front */
void dlist_clear(DList *l);          /* remove all nodes */
void dlist_free(DList *l);
