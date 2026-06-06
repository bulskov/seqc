#include "seqc/list.h"

#include <string.h>

typedef struct ListNode ListNode;

struct ListNode
{
    ListNode *next;
};

struct List
{
    ListNode *head;
    ListNode *tail;
    size_t len;
    size_t elem_size;
    allocator_t allocator;
};

/* Data lives immediately after the node header, padded to max_align_t so any
 * element type is correctly aligned. */
static void *node_data(const ListNode *node)
{
    const size_t offset = (sizeof(ListNode) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size)
{
    const size_t offset = (sizeof(ListNode) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return offset + elem_size;
}

static ListNode *list_make_node(const List *l, const void *elem)
{
    ListNode *node = mem_alloc(
        l->allocator, node_alloc_size(l->elem_size), _Alignof(max_align_t));
    if (!node)
        return NULL;
    node->next = NULL;
    memcpy(node_data(node), elem, l->elem_size);
    return node;
}

List *list_create(size_t elem_size, allocator_t allocator)
{
    List *l = mem_alloc(allocator, sizeof(List), _Alignof(List));
    if (!l)
        return NULL;
    *l = (List){.head = NULL,
                .tail = NULL,
                .len = 0,
                .elem_size = elem_size,
                .allocator = allocator};
    return l;
}

SeqcStatus list_push_front(List *l, const void *elem)
{
    if (!l || !elem)
        return SEQC_INVALID;
    ListNode *node = list_make_node(l, elem);
    if (!node)
        return SEQC_OOM;
    node->next = l->head;
    l->head = node;
    if (!l->tail)
        l->tail = node;
    l->len++;
    return SEQC_OK;
}

SeqcStatus list_push_back(List *l, const void *elem)
{
    if (!l || !elem)
        return SEQC_INVALID;
    ListNode *node = list_make_node(l, elem);
    if (!node)
        return SEQC_OOM;
    if (l->tail)
        l->tail->next = node;
    else
        l->head = node;
    l->tail = node;
    l->len++;
    return SEQC_OK;
}

SeqcStatus list_pop_front(List *l, void *out)
{
    if (!l || !l->head)
        return SEQC_NOT_FOUND;
    ListNode *node = l->head;
    if (out)
        memcpy(out, node_data(node), l->elem_size);
    l->head = node->next;
    if (!l->head)
        l->tail = NULL;
    mem_free(l->allocator, node, node_alloc_size(l->elem_size));
    l->len--;
    return SEQC_OK;
}

SeqcStatus list_pop_back(List *l, void *out)
{
    if (!l || !l->head)
        return SEQC_NOT_FOUND;
    if (l->head == l->tail)
        return list_pop_front(l, out);
    /* find second-to-last node */
    ListNode *prev = l->head;
    while (prev->next != l->tail)
        prev = prev->next;
    if (out)
        memcpy(out, node_data(l->tail), l->elem_size);
    mem_free(l->allocator, l->tail, node_alloc_size(l->elem_size));
    prev->next = NULL;
    l->tail = prev;
    l->len--;
    return SEQC_OK;
}

void *list_front(const List *l)
{
    return (l && l->head) ? node_data(l->head) : NULL;
}

void *list_back(const List *l)
{
    return (l && l->tail) ? node_data(l->tail) : NULL;
}

bool list_is_empty(const List *l)
{
    return !l || l->len == 0;
}

size_t list_len(const List *l)
{
    return l ? l->len : 0;
}

void list_clear(List *l)
{
    if (!l)
        return;
    ListNode *cur = l->head;
    while (cur)
    {
        ListNode *next = cur->next;
        mem_free(l->allocator, cur, node_alloc_size(l->elem_size));
        cur = next;
    }
    l->head = l->tail = NULL;
    l->len = 0;
}

void list_free(List *l)
{
    if (!l)
        return;
    list_clear(l);
    allocator_t al = l->allocator;
    mem_free(al, l, sizeof(List));
}

/* ---- iter -------------------------------------------------------------- */

typedef struct
{
    ListNode *current;
    size_t elem_size;
} ListIterState;

static bool list_iter_next(Iter *it, void *out)
{
    ListIterState *s = it->state;
    if (!s->current)
        return false;
    memcpy(out, node_data(s->current), s->elem_size);
    s->current = s->current->next;
    return true;
}

static void list_iter_drop(Iter *it)
{
    mem_free(it->allocator, it->state, sizeof(ListIterState));
}

Iter list_iter(const List *l)
{
    if (!l)
        return (Iter){0};
    ListIterState *s =
        mem_alloc(l->allocator, sizeof *s, _Alignof(ListIterState));
    if (!s)
        return (Iter){0};
    *s = (ListIterState){l->head, l->elem_size};
    return (Iter){.next = list_iter_next,
                  .drop = list_iter_drop,
                  .state = s,
                  .elem_size = l->elem_size,
                  .allocator = l->allocator};
}
