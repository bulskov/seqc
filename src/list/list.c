#include "seqc/list.h"

#include <string.h>

typedef struct list_node_t list_node_t;

struct list_node_t
{
    list_node_t *next;
};

struct list_t
{
    list_node_t *head;
    list_node_t *tail;
    size_t len;
    size_t elem_size;
    allocator_t allocator;
};

/* Data lives immediately after the node header, padded to max_align_t so any
 * element type is correctly aligned. */
static void *node_data(const list_node_t *node)
{
    const size_t offset = (sizeof(list_node_t) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size)
{
    const size_t offset = (sizeof(list_node_t) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return offset + elem_size;
}

static list_node_t *list_make_node(const list_t *l, const void *elem)
{
    list_node_t *node = mem_alloc(
        l->allocator, node_alloc_size(l->elem_size), _Alignof(max_align_t));
    if (!node)
        return NULL;
    node->next = NULL;
    memcpy(node_data(node), elem, l->elem_size);
    return node;
}

list_t *list_create(size_t elem_size, allocator_t allocator)
{
    list_t *l = mem_alloc(allocator, sizeof(list_t), _Alignof(list_t));
    if (!l)
        return NULL;
    *l = (list_t){.head = NULL,
                .tail = NULL,
                .len = 0,
                .elem_size = elem_size,
                .allocator = allocator};
    return l;
}

seqc_status_t list_push_front(list_t *l, const void *elem)
{
    if (!l || !elem)
        return SEQC_INVALID;
    list_node_t *node = list_make_node(l, elem);
    if (!node)
        return SEQC_OOM;
    node->next = l->head;
    l->head = node;
    if (!l->tail)
        l->tail = node;
    l->len++;
    return SEQC_OK;
}

seqc_status_t list_push_back(list_t *l, const void *elem)
{
    if (!l || !elem)
        return SEQC_INVALID;
    list_node_t *node = list_make_node(l, elem);
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

seqc_status_t list_pop_front(list_t *l, void *out)
{
    if (!l || !l->head)
        return SEQC_NOT_FOUND;
    list_node_t *node = l->head;
    if (out)
        memcpy(out, node_data(node), l->elem_size);
    l->head = node->next;
    if (!l->head)
        l->tail = NULL;
    mem_free(l->allocator, node, node_alloc_size(l->elem_size));
    l->len--;
    return SEQC_OK;
}

seqc_status_t list_pop_back(list_t *l, void *out)
{
    if (!l || !l->head)
        return SEQC_NOT_FOUND;
    if (l->head == l->tail)
        return list_pop_front(l, out);
    /* find second-to-last node */
    list_node_t *prev = l->head;
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

void *list_front(const list_t *l)
{
    return (l && l->head) ? node_data(l->head) : NULL;
}

void *list_back(const list_t *l)
{
    return (l && l->tail) ? node_data(l->tail) : NULL;
}

bool list_is_empty(const list_t *l)
{
    return !l || l->len == 0;
}

size_t list_len(const list_t *l)
{
    return l ? l->len : 0;
}

void list_clear(list_t *l)
{
    if (!l)
        return;
    list_node_t *cur = l->head;
    while (cur)
    {
        list_node_t *next = cur->next;
        mem_free(l->allocator, cur, node_alloc_size(l->elem_size));
        cur = next;
    }
    l->head = l->tail = NULL;
    l->len = 0;
}

void list_free(list_t *l)
{
    if (!l)
        return;
    list_clear(l);
    allocator_t al = l->allocator;
    mem_free(al, l, sizeof(list_t));
}

/* ---- iter -------------------------------------------------------------- */

typedef struct
{
    list_node_t *current;
    size_t elem_size;
} list_iter_state_t;

static bool list_iter_next(iter_t *it, void *out)
{
    list_iter_state_t *s = it->state;
    if (!s->current)
        return false;
    memcpy(out, node_data(s->current), s->elem_size);
    s->current = s->current->next;
    return true;
}

static void list_iter_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(list_iter_state_t));
}

iter_t list_iter(const list_t *l)
{
    if (!l)
        return (iter_t){0};
    list_iter_state_t *s =
        mem_alloc(l->allocator, sizeof *s, _Alignof(list_iter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (list_iter_state_t){l->head, l->elem_size};
    return (iter_t){.next = list_iter_next,
                  .drop = list_iter_drop,
                  .state = s,
                  .elem_size = l->elem_size,
                  .allocator = l->allocator};
}
