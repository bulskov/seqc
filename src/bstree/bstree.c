#include "seqc/bstree.h"

#include <stdbool.h>
#include <string.h>

typedef struct BSTreeNode BSTreeNode;
struct BSTreeNode
{
    BSTreeNode *left;
    BSTreeNode *right;
    /* element data stored inline after the header */
};

struct BSTree
{
    BSTreeNode *root;
    size_t len;
    size_t elem_size;
    compare_fn cmp;
    allocator_t allocator;
};

/* ---- node layout ------------------------------------------------------- */

static void *node_data(const BSTreeNode *node)
{
    const size_t offset = (sizeof(BSTreeNode) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size)
{
    const size_t offset = (sizeof(BSTreeNode) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return offset + elem_size;
}

static BSTreeNode *make_node(const BSTree *t, const void *elem)
{
    BSTreeNode *node = mem_alloc(
        t->allocator, node_alloc_size(t->elem_size), _Alignof(max_align_t));
    if (!node)
        return NULL;
    node->left = node->right = NULL;
    memcpy(node_data(node), elem, t->elem_size);
    return node;
}

/* ---- public API -------------------------------------------------------- */

BSTree *bstree_create(size_t elem_size, compare_fn cmp, allocator_t allocator)
{
    BSTree *t = mem_alloc(allocator, sizeof(BSTree), _Alignof(BSTree));
    if (!t)
        return NULL;
    *t = (BSTree){.root = NULL,
                  .len = 0,
                  .elem_size = elem_size,
                  .cmp = cmp,
                  .allocator = allocator};
    return t;
}

SeqcStatus bstree_insert(BSTree *t, const void *elem)
{
    if (!t || !elem)
        return SEQC_INVALID;
    BSTreeNode **cur = &t->root;
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

bool bstree_contains(const BSTree *t, const void *elem)
{
    if (!t || !elem)
        return false;
    BSTreeNode *cur = t->root;
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
static BSTreeNode *do_remove(
    BSTree *t, BSTreeNode *node, const void *elem, int *removed)
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
            BSTreeNode *r = node->right;
            mem_free(t->allocator, node, node_alloc_size(t->elem_size));
            return r;
        }
        if (!node->right)
        {
            BSTreeNode *l = node->left;
            mem_free(t->allocator, node, node_alloc_size(t->elem_size));
            return l;
        }
        /* two children: replace with in-order successor (min of right subtree),
         * copy its data here, then delete the successor below. */
        BSTreeNode *succ = node->right;
        while (succ->left)
            succ = succ->left;
        memcpy(node_data(node), node_data(succ), t->elem_size);
        int dummy = 0;
        node->right = do_remove(t, node->right, node_data(node), &dummy);
    }
    return node;
}

SeqcStatus bstree_remove(BSTree *t, const void *elem)
{
    if (!t || !elem)
        return SEQC_INVALID;
    int removed = 0;
    t->root = do_remove(t, t->root, elem, &removed);
    if (removed)
        t->len--;
    return removed ? SEQC_OK : SEQC_NOT_FOUND;
}

void *bstree_min(const BSTree *t)
{
    if (!t || !t->root)
        return NULL;
    BSTreeNode *cur = t->root;
    while (cur->left)
        cur = cur->left;
    return node_data(cur);
}

void *bstree_max(const BSTree *t)
{
    if (!t || !t->root)
        return NULL;
    BSTreeNode *cur = t->root;
    while (cur->right)
        cur = cur->right;
    return node_data(cur);
}

size_t bstree_len(const BSTree *t)
{
    return t ? t->len : 0;
}

static int bstree_height_node(const BSTreeNode *n)
{
    if (!n)
        return 0;
    int l = bstree_height_node(n->left);
    int r = bstree_height_node(n->right);
    return 1 + (l > r ? l : r);
}

int bstree_height(const BSTree *t)
{
    return t ? bstree_height_node(t->root) : 0;
}

static void free_subtree(BSTree *t, BSTreeNode *node)
{
    if (!node)
        return;
    free_subtree(t, node->left);
    free_subtree(t, node->right);
    mem_free(t->allocator, node, node_alloc_size(t->elem_size));
}

void bstree_free(BSTree *t)
{
    if (!t)
        return;
    free_subtree(t, t->root);
    allocator_t al = t->allocator;
    mem_free(al, t, sizeof(BSTree));
}

void bstree_clear(BSTree *t)
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
    BSTreeNode **stack;
    size_t stack_len;
    size_t stack_cap;
    BSTreeNode *current; /* next node to push */
    size_t elem_size;
    allocator_t allocator;
} BSTreeIterState;

static bool bstree_iter_next(Iter *it, void *out)
{
    BSTreeIterState *s = it->state;
    /* push left spine of current node, then pop and yield */
    while (s->current)
    {
        if (s->stack_len == s->stack_cap)
        {
            size_t new_cap = s->stack_cap == 0 ? BTREE_ITER_STACK_INIT_CAP
                                               : s->stack_cap * 2;
            s->stack = mem_realloc(
                s->allocator,

                s->stack,
                s->stack_cap * sizeof(BSTreeNode *),
                new_cap * sizeof(BSTreeNode *),
                _Alignof(BSTreeNode *));
            s->stack_cap = new_cap;
        }
        s->stack[s->stack_len++] = s->current;
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    BSTreeNode *node = s->stack[--s->stack_len];
    memcpy(out, node_data(node), s->elem_size);
    s->current = node->right;
    return true;
}

static void bstree_iter_drop(Iter *it)
{
    BSTreeIterState *s = it->state;
    if (s->stack)
        mem_free(it->allocator, s->stack, s->stack_cap * sizeof(BSTreeNode *));
    mem_free(it->allocator, s, sizeof(BSTreeIterState));
}

Iter bstree_iter(const BSTree *t)
{
    if (!t)
        return (Iter){0};
    BSTreeIterState *s =
        mem_alloc(t->allocator, sizeof *s, _Alignof(BSTreeIterState));
    *s = (BSTreeIterState){.stack = NULL,
                           .stack_len = 0,
                           .stack_cap = 0,
                           .current = t->root,
                           .elem_size = t->elem_size,
                           .allocator = t->allocator};
    return (Iter){.next = bstree_iter_next,
                  .drop = bstree_iter_drop,
                  .state = s,
                  .elem_size = t->elem_size,
                  .allocator = t->allocator};
}

static bool bstree_iter_rev_next(Iter *it, void *out)
{
    BSTreeIterState *s = it->state;
    while (s->current)
    {
        if (s->stack_len == s->stack_cap)
        {
            size_t new_cap = s->stack_cap == 0 ? BTREE_ITER_STACK_INIT_CAP
                                               : s->stack_cap * 2;
            s->stack = mem_realloc(
                s->allocator,

                s->stack,
                s->stack_cap * sizeof(BSTreeNode *),
                new_cap * sizeof(BSTreeNode *),
                _Alignof(BSTreeNode *));
            s->stack_cap = new_cap;
        }
        s->stack[s->stack_len++] = s->current;
        s->current = s->current->right;
    }
    if (s->stack_len == 0)
        return false;
    BSTreeNode *node = s->stack[--s->stack_len];
    memcpy(out, node_data(node), s->elem_size);
    s->current = node->left;
    return true;
}

Iter bstree_iter_rev(const BSTree *t)
{
    if (!t)
        return (Iter){0};
    BSTreeIterState *s =
        mem_alloc(t->allocator, sizeof *s, _Alignof(BSTreeIterState));
    *s = (BSTreeIterState){.stack = NULL,
                           .stack_len = 0,
                           .stack_cap = 0,
                           .current = t->root,
                           .elem_size = t->elem_size,
                           .allocator = t->allocator};
    return (Iter){.next = bstree_iter_rev_next,
                  .drop = bstree_iter_drop,
                  .state = s,
                  .elem_size = t->elem_size,
                  .allocator = t->allocator};
}

/* ---- range iterator ---------------------------------------------------- */

typedef struct
{
    BSTreeNode **stack;
    size_t stack_len;
    size_t stack_cap;
    BSTreeNode *current;
    size_t elem_size;
    allocator_t allocator;
    compare_fn cmp;
    void *hi; /* NULL means no upper bound; owned allocation */
} BSTreeRangeIterState;

static bool bstree_range_iter_next(Iter *it, void *out)
{
    BSTreeRangeIterState *s = it->state;
    while (s->current)
    {
        if (s->stack_len == s->stack_cap)
        {
            size_t new_cap = s->stack_cap == 0 ? BTREE_ITER_STACK_INIT_CAP
                                               : s->stack_cap * 2;
            s->stack = mem_realloc(
                s->allocator,

                s->stack,
                s->stack_cap * sizeof(BSTreeNode *),
                new_cap * sizeof(BSTreeNode *),
                _Alignof(BSTreeNode *));
            s->stack_cap = new_cap;
        }
        s->stack[s->stack_len++] = s->current;
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    BSTreeNode *node = s->stack[--s->stack_len];
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

static void bstree_range_iter_drop(Iter *it)
{
    BSTreeRangeIterState *s = it->state;
    if (s->stack)
        mem_free(it->allocator, s->stack, s->stack_cap * sizeof(BSTreeNode *));
    if (s->hi)
        mem_free(it->allocator, s->hi, s->elem_size);
    mem_free(it->allocator, s, sizeof(BSTreeRangeIterState));
}

static void bstree_push_lo(
    BSTreeRangeIterState *s, BSTreeNode *node, const void *lo)
{
    while (node)
    {
        if (s->cmp(node_data(node), lo) < 0)
        {
            node = node->right;
        }
        else
        {
            if (s->stack_len == s->stack_cap)
            {
                size_t new_cap = s->stack_cap == 0 ? BTREE_ITER_STACK_INIT_CAP
                                                   : s->stack_cap * 2;
                s->stack = mem_realloc(
                    s->allocator,

                    s->stack,
                    s->stack_cap * sizeof(BSTreeNode *),
                    new_cap * sizeof(BSTreeNode *),
                    _Alignof(BSTreeNode *));
                s->stack_cap = new_cap;
            }
            s->stack[s->stack_len++] = node;
            node = node->left;
        }
    }
}

Iter bstree_iter_range(const BSTree *t, const void *lo, const void *hi)
{
    if (!t)
        return (Iter){0};
    BSTreeRangeIterState *s =
        mem_alloc(t->allocator, sizeof *s, _Alignof(BSTreeRangeIterState));
    void *hi_copy = NULL;
    if (hi)
    {
        hi_copy = mem_alloc(t->allocator, t->elem_size, _Alignof(max_align_t));
        memcpy(hi_copy, hi, t->elem_size);
    }
    *s = (BSTreeRangeIterState){.stack = NULL,
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
    return (Iter){.next = bstree_range_iter_next,
                  .drop = bstree_range_iter_drop,
                  .state = s,
                  .elem_size = t->elem_size,
                  .allocator = t->allocator};
}
