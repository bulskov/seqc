#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

/* Singly-linked list.  Element data is stored inline immediately after each
 * node header, aligned to max_align_t. */

typedef struct list_t list_t;

list_t *list_create(size_t elem_size, allocator_t allocator);
seqc_status_t list_push_front(list_t *l, const void *elem);
seqc_status_t list_push_back(list_t *l, const void *elem);
seqc_status_t list_pop_front(
    list_t *l, void *out); /* SEQC_NOT_FOUND if empty; out may be NULL */
seqc_status_t list_pop_back(
    list_t *l, void *out); /* O(n) — prefer dlist for frequent back-pops */
/* The first / last element.  Copies into out (may be NULL to test only), so it
 * is not affected by later changes.  SEQC_NOT_FOUND if empty. */
seqc_status_t list_front(const list_t *l, void *out);
seqc_status_t list_back(const list_t *l, void *out);
/* Pointers to the first / last element, NULL if empty.  Nodes never move:
 * valid until that element is removed or the list is cleared/destroyed. */
void *list_front_ptr(const list_t *l);
void *list_back_ptr(const list_t *l);
bool list_is_empty(const list_t *l);
size_t list_len(const list_t *l);
iter_t list_iter(const list_t *l); /* front→back */
/* Push every element of it (consumed) at the back; stops at the first
 * error. */
seqc_status_t list_extend(list_t *l, iter_t it);
void list_clear(list_t *l); /* remove all nodes */
void list_destroy(list_t *l);
