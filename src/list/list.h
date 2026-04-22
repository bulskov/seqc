#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* Singly-linked list.  Element data is stored inline immediately after each
 * node header, aligned to max_align_t. */

 

typedef struct List List;

List *list_create(size_t elem_size, Allocator allocator);
void list_push_front(List *l, const void *elem);
void list_push_back(List *l, const void *elem);
bool list_pop_front(
    List *l, void *out); /* true=ok false=empty; out may be NULL */
bool list_pop_back(
    List *l, void *out); /* O(n) — prefer dlist for frequent back-pops */
void *list_front(const List *l); /* pointer to head data; NULL if empty */
void *list_back(const List *l);  /* pointer to tail data; NULL if empty */
bool list_is_empty(const List *l);
size_t list_len(const List *l);
Iter list_iter(const List *l); /* front→back */
void list_clear(List *l);      /* remove all nodes */
void list_free(List *l);
