#include "dlist.h"

#include <stdbool.h>
#include <string.h>

typedef struct DListNode DListNode;
struct DListNode
{
    DListNode *prev;
    DListNode *next;
};

struct DList
{
    DListNode *head;
    DListNode *tail;
    size_t len;
    size_t elem_size;
    Allocator allocator;
};

/* Data lives immediately after the node header, padded to max_align_t. */
static void *node_data(const DListNode *node)
{
    const size_t offset = (sizeof(DListNode) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size)
{
    const size_t offset = (sizeof(DListNode) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return offset + elem_size;
}

static DListNode *make_node(const DList *l, const void *elem)
{
    DListNode *node = l->allocator.alloc(
        l->allocator.ctx, node_alloc_size(l->elem_size), _Alignof(max_align_t));
    if (!node)
        return NULL;
    node->prev = NULL;
    node->next = NULL;
    memcpy(node_data(node), elem, l->elem_size);
    return node;
}

static void unlink_and_free(DList *l, DListNode *node)
{
    if (node->prev)
        node->prev->next = node->next;
    else
        l->head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else
        l->tail = node->prev;
    if (l->allocator.free)
        l->allocator.free(l->allocator.ctx, node);
    l->len--;
}

DList *dlist_create(size_t elem_size, Allocator allocator)
{
    DList *l = allocator.alloc(allocator.ctx, sizeof(DList), _Alignof(DList));
    if (!l)
        return NULL;
    *l = (DList){
        .head = NULL,
        .tail = NULL,
        .len = 0,
        .elem_size = elem_size,
        .allocator = allocator};
    return l;
}

SeqcStatus dlist_push_front(DList *l, const void *elem)
{
    if (!l || !elem)
        return SEQC_INVALID;
    DListNode *node = make_node(l, elem);
    if (!node)
        return SEQC_OOM;
    node->next = l->head;
    if (l->head)
        l->head->prev = node;
    else
        l->tail = node;
    l->head = node;
    l->len++;
    return SEQC_OK;
}

SeqcStatus dlist_push_back(DList *l, const void *elem)
{
    if (!l || !elem)
        return SEQC_INVALID;
    DListNode *node = make_node(l, elem);
    if (!node)
        return SEQC_OOM;
    node->prev = l->tail;
    if (l->tail)
        l->tail->next = node;
    else
        l->head = node;
    l->tail = node;
    l->len++;
    return SEQC_OK;
}

SeqcStatus dlist_pop_front(DList *l, void *out)
{
    if (!l || !l->head)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, node_data(l->head), l->elem_size);
    unlink_and_free(l, l->head);
    return SEQC_OK;
}

SeqcStatus dlist_pop_back(DList *l, void *out)
{
    if (!l || !l->tail)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, node_data(l->tail), l->elem_size);
    unlink_and_free(l, l->tail);
    return SEQC_OK;
}

void *dlist_front(const DList *l)
{
    return (l && l->head) ? node_data(l->head) : NULL;
}

void *dlist_back(const DList *l)
{
    return (l && l->tail) ? node_data(l->tail) : NULL;
}

bool dlist_is_empty(const DList *l)
{
    return !l || l->len == 0;
}

size_t dlist_len(const DList *l)
{
    return l ? l->len : 0;
}

void dlist_clear(DList *l)
{
    if (!l)
        return;
    DListNode *cur = l->head;
    while (cur)
    {
        DListNode *next = cur->next;
        if (l->allocator.free)
            l->allocator.free(l->allocator.ctx, cur);
        cur = next;
    }
    l->head = l->tail = NULL;
    l->len = 0;
}

void dlist_free(DList *l)
{
    if (!l)
        return;
    dlist_clear(l);
    Allocator al = l->allocator;
    if (al.free)
        al.free(al.ctx, l);
}

/* ---- iter (forward) ---------------------------------------------------- */

typedef struct
{
    DListNode *current;
    size_t elem_size;
} DListIterState;

static bool dlist_iter_next_fwd(Iter *it, void *out)
{
    DListIterState *s = it->state;
    if (!s->current)
        return false;
    memcpy(out, node_data(s->current), s->elem_size);
    s->current = s->current->next;
    return true;
}

static bool dlist_iter_next_rev(Iter *it, void *out)
{
    DListIterState *s = it->state;
    if (!s->current)
        return false;
    memcpy(out, node_data(s->current), s->elem_size);
    s->current = s->current->prev;
    return true;
}

static void dlist_iter_drop(Iter *it)
{
    if (it->allocator.free)
        it->allocator.free(it->allocator.ctx, it->state);
}

Iter dlist_iter(const DList *l)
{
    if (!l)
        return (Iter){0};
    DListIterState *s = l->allocator.alloc(
        l->allocator.ctx, sizeof *s, _Alignof(DListIterState));
    *s = (DListIterState){l->head, l->elem_size};
    return (Iter){
        .next = dlist_iter_next_fwd,
        .drop = dlist_iter_drop,
        .state = s,
        .elem_size = l->elem_size,
        .allocator = l->allocator};
}

Iter dlist_iter_reverse(const DList *l)
{
    if (!l)
        return (Iter){0};
    DListIterState *s = l->allocator.alloc(
        l->allocator.ctx, sizeof *s, _Alignof(DListIterState));
    *s = (DListIterState){l->tail, l->elem_size};
    return (Iter){
        .next = dlist_iter_next_rev,
        .drop = dlist_iter_drop,
        .state = s,
        .elem_size = l->elem_size,
        .allocator = l->allocator};
}
