#include "seqc/bstree.h"

#include <stdbool.h>
#include <string.h>

typedef struct bstree_node_t bstree_node_t;
struct bstree_node_t
{
    bstree_node_t *left;
    bstree_node_t *right;
    /* element data stored inline after the header */
};

struct bstree_t
{
    bstree_node_t *root;
    size_t len;
    size_t elem_size;
    compare_fn cmp;
    allocator_t allocator;
};

/* ---- node layout ------------------------------------------------------- */

static void *node_data(const bstree_node_t *node)
{
    const size_t offset = (sizeof(bstree_node_t) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size)
{
    const size_t offset = (sizeof(bstree_node_t) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return offset + elem_size;
}

static bstree_node_t *make_node(const bstree_t *t, const void *elem)
{
    bstree_node_t *node = mem_alloc(
        t->allocator, node_alloc_size(t->elem_size), _Alignof(max_align_t));
    if (!node)
        return NULL;
    node->left = node->right = NULL;
    memcpy(node_data(node), elem, t->elem_size);
    return node;
}

/* ---- public API -------------------------------------------------------- */

bstree_t *bstree_create(size_t elem_size, compare_fn cmp, allocator_t allocator)
{
    bstree_t *t = mem_alloc(allocator, sizeof(bstree_t), _Alignof(bstree_t));
    if (!t)
        return NULL;
    *t = (bstree_t){.root = NULL,
                  .len = 0,
                  .elem_size = elem_size,
                  .cmp = cmp,
                  .allocator = allocator};
    return t;
}

seqc_status_t bstree_insert(bstree_t *t, const void *elem)
{
    if (!t || !elem)
        return SEQC_INVALID;
    bstree_node_t **cur = &t->root;
    while (*cur)
    {
        int c = t->cmp(elem, node_data(*cur));
        if (c < 0)
            cur = &(*cur)->left;
        else if (c > 0)
            cur = &(*cur)->right;
        else
            return SEQC_DUPLICATE;
    }
    *cur = make_node(t, elem);
    if (!*cur)
        return SEQC_OOM;
    t->len++;
    return SEQC_OK;
}

bool bstree_contains(const bstree_t *t, const void *elem)
{
    if (!t || !elem)
        return false;
    bstree_node_t *cur = t->root;
    while (cur)
    {
        int c = t->cmp(elem, node_data(cur));
        if (c < 0)
            cur = cur->left;
        else if (c > 0)
            cur = cur->right;
        else
            return true;
    }
    return false;
}

/* Recursive helper — returns updated subtree root. */
static bstree_node_t *do_remove(
    bstree_t *t, bstree_node_t *node, const void *elem, int *removed)
{
    if (!node)
    {
        *removed = 0;
        return NULL;
    }
    int c = t->cmp(elem, node_data(node));
    if (c < 0)
    {
        node->left = do_remove(t, node->left, elem, removed);
    }
    else if (c > 0)
    {
        node->right = do_remove(t, node->right, elem, removed);
    }
    else
    {
        *removed = 1;
        if (!node->left)
        {
            bstree_node_t *r = node->right;
            mem_free(t->allocator, node, node_alloc_size(t->elem_size));
            return r;
        }
        if (!node->right)
        {
            bstree_node_t *l = node->left;
            mem_free(t->allocator, node, node_alloc_size(t->elem_size));
            return l;
        }
        /* two children: replace with in-order successor (min of right subtree),
         * copy its data here, then delete the successor below. */
        bstree_node_t *succ = node->right;
        while (succ->left)
            succ = succ->left;
        memcpy(node_data(node), node_data(succ), t->elem_size);
        int dummy = 0;
        node->right = do_remove(t, node->right, node_data(node), &dummy);
    }
    return node;
}

seqc_status_t bstree_remove(bstree_t *t, const void *elem)
{
    if (!t || !elem)
        return SEQC_INVALID;
    int removed = 0;
    t->root = do_remove(t, t->root, elem, &removed);
    if (removed)
        t->len--;
    return removed ? SEQC_OK : SEQC_NOT_FOUND;
}

void *bstree_min(const bstree_t *t)
{
    if (!t || !t->root)
        return NULL;
    bstree_node_t *cur = t->root;
    while (cur->left)
        cur = cur->left;
    return node_data(cur);
}

void *bstree_max(const bstree_t *t)
{
    if (!t || !t->root)
        return NULL;
    bstree_node_t *cur = t->root;
    while (cur->right)
        cur = cur->right;
    return node_data(cur);
}

size_t bstree_len(const bstree_t *t)
{
    return t ? t->len : 0;
}

static int bstree_height_node(const bstree_node_t *n)
{
    if (!n)
        return 0;
    int l = bstree_height_node(n->left);
    int r = bstree_height_node(n->right);
    return 1 + (l > r ? l : r);
}

int bstree_height(const bstree_t *t)
{
    return t ? bstree_height_node(t->root) : 0;
}

static void free_subtree(bstree_t *t, bstree_node_t *node)
{
    if (!node)
        return;
    free_subtree(t, node->left);
    free_subtree(t, node->right);
    mem_free(t->allocator, node, node_alloc_size(t->elem_size));
}

void bstree_free(bstree_t *t)
{
    if (!t)
        return;
    free_subtree(t, t->root);
    allocator_t al = t->allocator;
    mem_free(al, t, sizeof(bstree_t));
}

void bstree_clear(bstree_t *t)
{
    if (!t)
        return;
    free_subtree(t, t->root);
    t->root = NULL;
    t->len = 0;
}

/* ---- iter: iterative in-order ----------------------------------------- */

#define BTREE_ITER_STACK_INIT_CAP 16

typedef struct
{
    bstree_node_t **stack;
    size_t stack_len;
    size_t stack_cap;
    bstree_node_t *current; /* next node to push */
    size_t elem_size;
    allocator_t allocator;
} bstree_iter_state_t;

/* Push node onto a growable traversal stack, doubling capacity on demand.
 * Returns false on OOM, leaving the existing stack intact for the drop. */
static bool bstree_stack_push(bstree_node_t ***stack, size_t *len, size_t *cap,
                              allocator_t al, bstree_node_t *node)
{
    if (*len == *cap)
    {
        size_t new_cap = *cap == 0 ? BTREE_ITER_STACK_INIT_CAP : *cap * 2;
        bstree_node_t **grown =
            mem_realloc(al, *stack, *cap * sizeof(bstree_node_t *),
                        new_cap * sizeof(bstree_node_t *), _Alignof(bstree_node_t *));
        if (!grown)
            return false;
        *stack = grown;
        *cap = new_cap;
    }
    (*stack)[(*len)++] = node;
    return true;
}

static bool bstree_iter_next(iter_t *it, void *out)
{
    bstree_iter_state_t *s = it->state;
    /* push left spine of current node, then pop and yield */
    while (s->current)
    {
        if (!bstree_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                               s->allocator, s->current))
            return false; /* OOM: end iteration; drop frees the stack */
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    bstree_node_t *node = s->stack[--s->stack_len];
    memcpy(out, node_data(node), s->elem_size);
    s->current = node->right;
    return true;
}

static void bstree_iter_drop(iter_t *it)
{
    bstree_iter_state_t *s = it->state;
    if (s->stack)
        mem_free(it->allocator, s->stack, s->stack_cap * sizeof(bstree_node_t *));
    mem_free(it->allocator, s, sizeof(bstree_iter_state_t));
}

iter_t bstree_iter(const bstree_t *t)
{
    if (!t)
        return (iter_t){0};
    bstree_iter_state_t *s =
        mem_alloc(t->allocator, sizeof *s, _Alignof(bstree_iter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (bstree_iter_state_t){.stack = NULL,
                           .stack_len = 0,
                           .stack_cap = 0,
                           .current = t->root,
                           .elem_size = t->elem_size,
                           .allocator = t->allocator};
    return (iter_t){.next = bstree_iter_next,
                  .drop = bstree_iter_drop,
                  .state = s,
                  .elem_size = t->elem_size,
                  .allocator = t->allocator};
}

static bool bstree_iter_rev_next(iter_t *it, void *out)
{
    bstree_iter_state_t *s = it->state;
    while (s->current)
    {
        if (!bstree_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                               s->allocator, s->current))
            return false; /* OOM: end iteration; drop frees the stack */
        s->current = s->current->right;
    }
    if (s->stack_len == 0)
        return false;
    bstree_node_t *node = s->stack[--s->stack_len];
    memcpy(out, node_data(node), s->elem_size);
    s->current = node->left;
    return true;
}

iter_t bstree_iter_rev(const bstree_t *t)
{
    if (!t)
        return (iter_t){0};
    bstree_iter_state_t *s =
        mem_alloc(t->allocator, sizeof *s, _Alignof(bstree_iter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (bstree_iter_state_t){.stack = NULL,
                           .stack_len = 0,
                           .stack_cap = 0,
                           .current = t->root,
                           .elem_size = t->elem_size,
                           .allocator = t->allocator};
    return (iter_t){.next = bstree_iter_rev_next,
                  .drop = bstree_iter_drop,
                  .state = s,
                  .elem_size = t->elem_size,
                  .allocator = t->allocator};
}

/* ---- range iterator ---------------------------------------------------- */

typedef struct
{
    bstree_node_t **stack;
    size_t stack_len;
    size_t stack_cap;
    bstree_node_t *current;
    size_t elem_size;
    allocator_t allocator;
    compare_fn cmp;
    void *hi; /* NULL means no upper bound; owned allocation */
} bstree_range_iter_state_t;

static bool bstree_range_iter_next(iter_t *it, void *out)
{
    bstree_range_iter_state_t *s = it->state;
    while (s->current)
    {
        if (!bstree_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                               s->allocator, s->current))
            return false; /* OOM: end iteration; drop frees the stack */
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    bstree_node_t *node = s->stack[--s->stack_len];
    void *data = node_data(node);
    if (s->hi && s->cmp(data, s->hi) > 0)
    {
        s->stack_len = 0;
        s->current = NULL;
        return false;
    }
    memcpy(out, data, s->elem_size);
    s->current = node->right;
    return true;
}

static void bstree_range_iter_drop(iter_t *it)
{
    bstree_range_iter_state_t *s = it->state;
    if (s->stack)
        mem_free(it->allocator, s->stack, s->stack_cap * sizeof(bstree_node_t *));
    if (s->hi)
        mem_free(it->allocator, s->hi, s->elem_size);
    mem_free(it->allocator, s, sizeof(bstree_range_iter_state_t));
}

static void bstree_push_lo(
    bstree_range_iter_state_t *s, bstree_node_t *node, const void *lo)
{
    while (node)
    {
        if (s->cmp(node_data(node), lo) < 0)
        {
            node = node->right;
        }
        else
        {
            if (!bstree_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                                   s->allocator, node))
                return; /* OOM: stop priming; iterator yields what it has */
            node = node->left;
        }
    }
}

iter_t bstree_iter_range(const bstree_t *t, const void *lo, const void *hi)
{
    if (!t)
        return (iter_t){0};
    bstree_range_iter_state_t *s =
        mem_alloc(t->allocator, sizeof *s, _Alignof(bstree_range_iter_state_t));
    if (!s)
        return (iter_t){0};
    void *hi_copy = NULL;
    if (hi)
    {
        hi_copy = mem_alloc(t->allocator, t->elem_size, _Alignof(max_align_t));
        if (!hi_copy)
        {
            mem_free(t->allocator, s, sizeof(bstree_range_iter_state_t));
            return (iter_t){0};
        }
        memcpy(hi_copy, hi, t->elem_size);
    }
    *s = (bstree_range_iter_state_t){.stack = NULL,
                                .stack_len = 0,
                                .stack_cap = 0,
                                .current = NULL,
                                .elem_size = t->elem_size,
                                .allocator = t->allocator,
                                .cmp = t->cmp,
                                .hi = hi_copy};
    if (lo)
        bstree_push_lo(s, t->root, lo);
    else
        s->current = t->root;
    return (iter_t){.next = bstree_range_iter_next,
                  .drop = bstree_range_iter_drop,
                  .state = s,
                  .elem_size = t->elem_size,
                  .allocator = t->allocator};
}
