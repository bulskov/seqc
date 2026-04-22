#pragma once

#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* Singly-linked list.  Element data is stored inline immediately after each
 * node header, aligned to max_align_t. */
typedef struct ListNode ListNode;
struct ListNode {
  ListNode *next;
};

typedef struct {
  ListNode *head;
  ListNode *tail;
  size_t len;
  size_t elem_size;
  Allocator allocator;
} List;

List list_create(size_t elem_size, Allocator allocator);
void list_push_front(List *l, const void *elem);
void list_push_back(List *l, const void *elem);
int list_pop_front(List *l, void *out); /* 1=ok 0=empty; out may be NULL */
void *list_front(const List *l); /* pointer to head data; NULL if empty */
void *list_back(const List *l);  /* pointer to tail data; NULL if empty */
int list_is_empty(const List *l);
size_t list_len(const List *l);
Iter list_iter(const List *l); /* front→back */
void list_free(List *l);
