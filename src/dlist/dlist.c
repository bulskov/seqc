#include "seqc/dlist.h"

#include <stdbool.h>
#include <string.h>

typedef struct dlist_node_t dlist_node_t;
struct dlist_node_t
{
    dlist_node_t *prev;
    dlist_node_t *next;
};

struct dlist_t
{
    dlist_node_t *head;
    dlist_node_t *tail;
    size_t len;
    size_t elem_size;
    allocator_t allocator;
};

/* Data lives immediately after the node header, padded to max_align_t. */
static void *node_data(const dlist_node_t *node)
{
    const size_t offset = (sizeof(dlist_node_t) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size)
{
    const size_t offset = (sizeof(dlist_node_t) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return offset + elem_size;
}

static dlist_node_t *make_node(const dlist_t *l, const void *elem)
{
    dlist_node_t *node = mem_alloc(
        l->allocator, node_alloc_size(l->elem_size), _Alignof(max_align_t));
    if (!node)
        return NULL;
    node->prev = NULL;
    node->next = NULL;
    memcpy(node_data(node), elem, l->elem_size);
    return node;
}

static void unlink_and_free(dlist_t *l, dlist_node_t *node)
{
    if (node->prev)
        node->prev->next = node->next;
    else
        l->head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else
        l->tail = node->prev;
    mem_free(l->allocator, node, node_alloc_size(l->elem_size));
    l->len--;
}

dlist_t *dlist_create(size_t elem_size, allocator_t allocator)
{
    dlist_t *l = mem_alloc(allocator, sizeof(dlist_t), _Alignof(dlist_t));
    if (!l)
        return NULL;
    *l = (dlist_t){.head = NULL,
                 .tail = NULL,
                 .len = 0,
                 .elem_size = elem_size,
                 .allocator = allocator};
    return l;
}

seqc_status_t dlist_push_front(dlist_t *l, const void *elem)
{
    if (!l || !elem)
        return SEQC_INVALID;
    dlist_node_t *node = make_node(l, elem);
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

seqc_status_t dlist_push_back(dlist_t *l, const void *elem)
{
    if (!l || !elem)
        return SEQC_INVALID;
    dlist_node_t *node = make_node(l, elem);
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

seqc_status_t dlist_pop_front(dlist_t *l, void *out)
{
    if (!l || !l->head)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, node_data(l->head), l->elem_size);
    unlink_and_free(l, l->head);
    return SEQC_OK;
}

seqc_status_t dlist_pop_back(dlist_t *l, void *out)
{
    if (!l || !l->tail)
        return SEQC_NOT_FOUND;
    if (out)
        memcpy(out, node_data(l->tail), l->elem_size);
    unlink_and_free(l, l->tail);
    return SEQC_OK;
}

void *dlist_front(const dlist_t *l)
{
    return (l && l->head) ? node_data(l->head) : NULL;
}

void *dlist_back(const dlist_t *l)
{
    return (l && l->tail) ? node_data(l->tail) : NULL;
}

bool dlist_is_empty(const dlist_t *l)
{
    return !l || l->len == 0;
}

size_t dlist_len(const dlist_t *l)
{
    return l ? l->len : 0;
}

void dlist_clear(dlist_t *l)
{
    if (!l)
        return;
    dlist_node_t *cur = l->head;
    while (cur)
    {
        dlist_node_t *next = cur->next;
        mem_free(l->allocator, cur, node_alloc_size(l->elem_size));
        cur = next;
    }
    l->head = l->tail = NULL;
    l->len = 0;
}

void dlist_free(dlist_t *l)
{
    if (!l)
        return;
    dlist_clear(l);
    allocator_t al = l->allocator;
    mem_free(al, l, sizeof(dlist_t));
}

/* ---- iter (forward) ---------------------------------------------------- */

typedef struct
{
    dlist_node_t *current;
    size_t elem_size;
} dlist_iter_state_t;

static bool dlist_iter_next_fwd(iter_t *it, void *out)
{
    dlist_iter_state_t *s = it->state;
    if (!s->current)
        return false;
    memcpy(out, node_data(s->current), s->elem_size);
    s->current = s->current->next;
    return true;
}

static bool dlist_iter_next_rev(iter_t *it, void *out)
{
    dlist_iter_state_t *s = it->state;
    if (!s->current)
        return false;
    memcpy(out, node_data(s->current), s->elem_size);
    s->current = s->current->prev;
    return true;
}

static void dlist_iter_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(dlist_iter_state_t));
}

iter_t dlist_iter(const dlist_t *l)
{
    if (!l)
        return (iter_t){0};
    dlist_iter_state_t *s =
        mem_alloc(l->allocator, sizeof *s, _Alignof(dlist_iter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (dlist_iter_state_t){l->head, l->elem_size};
    return (iter_t){.next = dlist_iter_next_fwd,
                  .drop = dlist_iter_drop,
                  .state = s,
                  .elem_size = l->elem_size,
                  .allocator = l->allocator};
}

iter_t dlist_iter_rev(const dlist_t *l)
{
    if (!l)
        return (iter_t){0};
    dlist_iter_state_t *s =
        mem_alloc(l->allocator, sizeof *s, _Alignof(dlist_iter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (dlist_iter_state_t){l->tail, l->elem_size};
    return (iter_t){.next = dlist_iter_next_rev,
                  .drop = dlist_iter_drop,
                  .state = s,
                  .elem_size = l->elem_size,
                  .allocator = l->allocator};
}
