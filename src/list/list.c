#include "list.h"

#include <string.h>

/* Data lives immediately after the node header, padded to max_align_t so any
 * element type is correctly aligned. */
static void *node_data(const ListNode *node) {
  const size_t offset = (sizeof(ListNode) + _Alignof(max_align_t) - 1) &
                        ~(_Alignof(max_align_t) - 1);
  return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size) {
  const size_t offset = (sizeof(ListNode) + _Alignof(max_align_t) - 1) &
                        ~(_Alignof(max_align_t) - 1);
  return offset + elem_size;
}

static ListNode *list_make_node(const List *l, const void *elem) {
  ListNode *node = l->allocator.alloc(
      l->allocator.ctx, node_alloc_size(l->elem_size), _Alignof(max_align_t));
  node->next = NULL;
  memcpy(node_data(node), elem, l->elem_size);
  return node;
}

List list_create(size_t elem_size, Allocator allocator) {
  return (List){.head = NULL,
                .tail = NULL,
                .len = 0,
                .elem_size = elem_size,
                .allocator = allocator};
}

void list_push_front(List *l, const void *elem) {
  if (!l || !elem)
    return;
  ListNode *node = list_make_node(l, elem);
  node->next = l->head;
  l->head = node;
  if (!l->tail)
    l->tail = node;
  l->len++;
}

void list_push_back(List *l, const void *elem) {
  if (!l || !elem)
    return;
  ListNode *node = list_make_node(l, elem);
  if (l->tail)
    l->tail->next = node;
  else
    l->head = node;
  l->tail = node;
  l->len++;
}

bool list_pop_front(List *l, void *out) {
  if (!l || !l->head)
    return false;
  ListNode *node = l->head;
  if (out)
    memcpy(out, node_data(node), l->elem_size);
  l->head = node->next;
  if (!l->head)
    l->tail = NULL;
  if (l->allocator.free)
    l->allocator.free(l->allocator.ctx, node);
  l->len--;
  return true;
}

void *list_front(const List *l) {
  return (l && l->head) ? node_data(l->head) : NULL;
}

void *list_back(const List *l) {
  return (l && l->tail) ? node_data(l->tail) : NULL;
}

bool list_is_empty(const List *l) { return !l || l->len == 0; }

size_t list_len(const List *l) { return l ? l->len : 0; }

void list_free(List *l) {
  if (!l)
    return;
  ListNode *cur = l->head;
  while (cur) {
    ListNode *next = cur->next;
    if (l->allocator.free)
      l->allocator.free(l->allocator.ctx, cur);
    cur = next;
  }
  l->head = l->tail = NULL;
  l->len = 0;
}

/* ---- iter -------------------------------------------------------------- */

typedef struct {
  ListNode *current;
  size_t elem_size;
} ListIterState;

static bool list_iter_next(Iter *it, void *out) {
  ListIterState *s = it->state;
  if (!s->current)
    return false;
  memcpy(out, node_data(s->current), s->elem_size);
  s->current = s->current->next;
  return true;
}

static void list_iter_drop(Iter *it) {
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, it->state);
}

Iter list_iter(const List *l) {
  if (!l)
    return (Iter){0};
  ListIterState *s =
      l->allocator.alloc(l->allocator.ctx, sizeof *s, _Alignof(ListIterState));
  *s = (ListIterState){l->head, l->elem_size};
  return (Iter){.next = list_iter_next,
                .drop = list_iter_drop,
                .state = s,
                .elem_size = l->elem_size,
                .allocator = l->allocator};
}
