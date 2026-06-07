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
void *list_front(const list_t *l); /* pointer to head data; NULL if empty */
void *list_back(const list_t *l);  /* pointer to tail data; NULL if empty */
bool list_is_empty(const list_t *l);
size_t list_len(const list_t *l);
iter_t list_iter(const list_t *l); /* front→back */
void list_clear(list_t *l);      /* remove all nodes */
void list_free(list_t *l);
