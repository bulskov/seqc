#include "avl.h"

#include <string.h>

typedef struct AVLNode AVLNode;
struct AVLNode
{
    AVLNode *left;
    AVLNode *right;
    int height; /* 1-based; 0 used as sentinel for NULL */
                /* element data stored inline after the header */
};

struct AVLTree
{
    AVLNode *root;
    size_t len;
    size_t elem_size;
    compare_fn cmp;
    Allocator allocator;
};

/* ---- node layout ------------------------------------------------------- */

static void *node_data(const AVLNode *node)
{
    const size_t offset = (sizeof(AVLNode) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size)
{
    const size_t offset = (sizeof(AVLNode) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return offset + elem_size;
}

static AVLNode *make_node(const AVLTree *t, const void *elem)
{
    AVLNode *n = t->allocator.alloc(
        t->allocator.ctx, node_alloc_size(t->elem_size), _Alignof(max_align_t));
    if (!n)
        return NULL;
    n->left = n->right = NULL;
    n->height = 1;
    memcpy(node_data(n), elem, t->elem_size);
    return n;
}

/* ---- AVL helpers ------------------------------------------------------- */

static int node_height(const AVLNode *n)
{
    return n ? n->height : 0;
}

static int max2(int a, int b)
{
    return a > b ? a : b;
}

static void update_height(AVLNode *n)
{
    n->height = 1 + max2(node_height(n->left), node_height(n->right));
}

static int balance_factor(const AVLNode *n)
{
    return node_height(n->left) - node_height(n->right);
}

/* ---- rotations --------------------------------------------------------- */

/*       y                 x
 *      / \              /   \
 *     x   C    -->    A       y
 *    / \                     / \
 *   A   B                   B   C   */
static AVLNode *rotate_right(AVLNode *y)
{
    AVLNode *x = y->left;
    AVLNode *b = x->right;
    x->right = y;
    y->left = b;
    update_height(y);
    update_height(x);
    return x;
}

/*     x                   y
 *    / \                /   \
 *   A   y    -->      x       C
 *      / \           / \
 *     B   C         A   B          */
static AVLNode *rotate_left(AVLNode *x)
{
    AVLNode *y = x->right;
    AVLNode *b = y->left;
    y->left = x;
    x->right = b;
    update_height(x);
    update_height(y);
    return y;
}

static AVLNode *rebalance(AVLNode *n)
{
    update_height(n);
    int bf = balance_factor(n);

    /* LL: right rotation */
    if (bf > 1 && balance_factor(n->left) >= 0)
        return rotate_right(n);

    /* LR: left-right double rotation */
    if (bf > 1 && balance_factor(n->left) < 0)
    {
        n->left = rotate_left(n->left);
        return rotate_right(n);
    }

    /* RR: left rotation */
    if (bf < -1 && balance_factor(n->right) <= 0)
        return rotate_left(n);

    /* RL: right-left double rotation */
    if (bf < -1 && balance_factor(n->right) > 0)
    {
        n->right = rotate_right(n->right);
        return rotate_left(n);
    }

    return n;
}

/* ---- public API -------------------------------------------------------- */

AVLTree *avl_create(size_t elem_size, compare_fn cmp, Allocator allocator)
{
    AVLTree *t =
        allocator.alloc(allocator.ctx, sizeof(AVLTree), _Alignof(AVLTree));
    if (!t)
        return NULL;
    *t = (AVLTree){
        .root = NULL,
        .len = 0,
        .elem_size = elem_size,
        .cmp = cmp,
        .allocator = allocator};
    return t;
}

/* --- insert ------------------------------------------------------------- */

static AVLNode *do_insert(
    AVLTree *t, AVLNode *node, const void *elem, bool *inserted, bool *oom)
{
    if (!node)
    {
        AVLNode *n = make_node(t, elem);
        if (!n) { *oom = true; return NULL; }
        *inserted = true;
        return n;
    }
    int c = t->cmp(elem, node_data(node));
    if (c < 0)
    {
        AVLNode *new_left = do_insert(t, node->left, elem, inserted, oom);
        if (*oom) return node; /* preserve existing tree on OOM */
        node->left = new_left;
    }
    else if (c > 0)
    {
        AVLNode *new_right = do_insert(t, node->right, elem, inserted, oom);
        if (*oom) return node;
        node->right = new_right;
    }
    else
    {
        *inserted = false;
        return node; /* duplicate — no rotation needed */
    }
    return rebalance(node);
}

SeqcStatus avl_insert(AVLTree *t, const void *elem)
{
    if (!t || !elem)
        return SEQC_INVALID;
    bool inserted = false;
    bool oom = false;
    AVLNode *new_root = do_insert(t, t->root, elem, &inserted, &oom);
    if (oom)
        return SEQC_OOM;
    if (new_root)
        t->root = new_root;
    if (inserted)
        t->len++;
    return inserted ? SEQC_OK : SEQC_DUPLICATE;
}

/* --- contains ----------------------------------------------------------- */

bool avl_contains(const AVLTree *t, const void *elem)
{
    if (!t || !elem)
        return false;
    AVLNode *cur = t->root;
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

/* --- remove ------------------------------------------------------------- */

static AVLNode *min_node(AVLNode *n)
{
    while (n->left)
        n = n->left;
    return n;
}

static AVLNode *do_remove(
    AVLTree *t, AVLNode *node, const void *elem, bool *removed)
{
    if (!node)
    {
        *removed = false;
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
        *removed = true;
        if (!node->left)
        {
            AVLNode *r = node->right;
            if (t->allocator.free)
                t->allocator.free(t->allocator.ctx, node);
            return r;
        }
        if (!node->right)
        {
            AVLNode *l = node->left;
            if (t->allocator.free)
                t->allocator.free(t->allocator.ctx, node);
            return l;
        }
        /* two children: overwrite with in-order successor then delete it below
         */
        AVLNode *succ = min_node(node->right);
        memcpy(node_data(node), node_data(succ), t->elem_size);
        bool dummy = false;
        node->right = do_remove(t, node->right, node_data(node), &dummy);
    }
    return rebalance(node);
}

SeqcStatus avl_remove(AVLTree *t, const void *elem)
{
    if (!t || !elem)
        return SEQC_INVALID;
    bool removed = false;
    t->root = do_remove(t, t->root, elem, &removed);
    if (removed)
        t->len--;
    return removed ? SEQC_OK : SEQC_NOT_FOUND;
}

/* --- min / max ---------------------------------------------------------- */

void *avl_min(const AVLTree *t)
{
    if (!t || !t->root)
        return NULL;
    AVLNode *cur = t->root;
    while (cur->left)
        cur = cur->left;
    return node_data(cur);
}

void *avl_max(const AVLTree *t)
{
    if (!t || !t->root)
        return NULL;
    AVLNode *cur = t->root;
    while (cur->right)
        cur = cur->right;
    return node_data(cur);
}

/* --- misc --------------------------------------------------------------- */

size_t avl_len(const AVLTree *t)
{
    return t ? t->len : 0;
}
int avl_height(const AVLTree *t)
{
    return t ? node_height(t->root) : 0;
}

static void free_subtree(AVLTree *t, AVLNode *node)
{
    if (!node)
        return;
    free_subtree(t, node->left);
    free_subtree(t, node->right);
    if (t->allocator.free)
        t->allocator.free(t->allocator.ctx, node);
}

void avl_free(AVLTree *t)
{
    if (!t)
        return;
    free_subtree(t, t->root);
    Allocator al = t->allocator;
    if (al.free)
        al.free(al.ctx, t);
}

void avl_clear(AVLTree *t)
{
    if (!t)
        return;
    free_subtree(t, t->root);
    t->root = NULL;
    t->len = 0;
}

/* ---- iter: iterative in-order ----------------------------------------- */

#define AVL_ITER_STACK_INIT_CAP 16

typedef struct
{
    AVLNode **stack;
    size_t stack_len;
    size_t stack_cap;
    AVLNode *current;
    size_t elem_size;
    Allocator allocator;
} AVLIterState;

static bool avl_iter_next(Iter *it, void *out)
{
    AVLIterState *s = it->state;
    while (s->current)
    {
        if (s->stack_len == s->stack_cap)
        {
            size_t new_cap =
                s->stack_cap == 0 ? AVL_ITER_STACK_INIT_CAP : s->stack_cap * 2;
            s->stack = s->allocator.realloc(
                s->allocator.ctx,
                s->stack,
                s->stack_cap * sizeof(AVLNode *),
                new_cap * sizeof(AVLNode *),
                _Alignof(AVLNode *));
            s->stack_cap = new_cap;
        }
        s->stack[s->stack_len++] = s->current;
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    AVLNode *node = s->stack[--s->stack_len];
    memcpy(out, node_data(node), s->elem_size);
    s->current = node->right;
    return true;
}

static void avl_iter_drop(Iter *it)
{
    AVLIterState *s = it->state;
    if (it->allocator.free)
    {
        if (s->stack)
            it->allocator.free(it->allocator.ctx, s->stack);
        it->allocator.free(it->allocator.ctx, s);
    }
}

Iter avl_iter(const AVLTree *t)
{
    if (!t)
        return (Iter){0};
    AVLIterState *s =
        t->allocator.alloc(t->allocator.ctx, sizeof *s, _Alignof(AVLIterState));
    *s = (AVLIterState){
        .stack = NULL,
        .stack_len = 0,
        .stack_cap = 0,
        .current = t->root,
        .elem_size = t->elem_size,
        .allocator = t->allocator};
    return (Iter){
        .next = avl_iter_next,
        .drop = avl_iter_drop,
        .state = s,
        .elem_size = t->elem_size,
        .allocator = t->allocator};
}

static bool avl_iter_rev_next(Iter *it, void *out)
{
    AVLIterState *s = it->state;
    while (s->current)
    {
        if (s->stack_len == s->stack_cap)
        {
            size_t new_cap =
                s->stack_cap == 0 ? AVL_ITER_STACK_INIT_CAP : s->stack_cap * 2;
            s->stack = s->allocator.realloc(
                s->allocator.ctx,
                s->stack,
                s->stack_cap * sizeof(AVLNode *),
                new_cap * sizeof(AVLNode *),
                _Alignof(AVLNode *));
            s->stack_cap = new_cap;
        }
        s->stack[s->stack_len++] = s->current;
        s->current = s->current->right;
    }
    if (s->stack_len == 0)
        return false;
    AVLNode *node = s->stack[--s->stack_len];
    memcpy(out, node_data(node), s->elem_size);
    s->current = node->left;
    return true;
}

Iter avl_iter_rev(const AVLTree *t)
{
    if (!t)
        return (Iter){0};
    AVLIterState *s =
        t->allocator.alloc(t->allocator.ctx, sizeof *s, _Alignof(AVLIterState));
    *s = (AVLIterState){
        .stack = NULL,
        .stack_len = 0,
        .stack_cap = 0,
        .current = t->root,
        .elem_size = t->elem_size,
        .allocator = t->allocator};
    return (Iter){
        .next = avl_iter_rev_next,
        .drop = avl_iter_drop,
        .state = s,
        .elem_size = t->elem_size,
        .allocator = t->allocator};
}

/* ---- range iterator ---------------------------------------------------- */

typedef struct
{
    AVLNode **stack;
    size_t stack_len;
    size_t stack_cap;
    AVLNode *current;
    size_t elem_size;
    Allocator allocator;
    compare_fn cmp;
    void *hi; /* NULL means no upper bound; owned allocation */
} AVLRangeIterState;

static bool avl_range_iter_next(Iter *it, void *out)
{
    AVLRangeIterState *s = it->state;
    while (s->current)
    {
        if (s->stack_len == s->stack_cap)
        {
            size_t new_cap =
                s->stack_cap == 0 ? AVL_ITER_STACK_INIT_CAP : s->stack_cap * 2;
            s->stack = s->allocator.realloc(
                s->allocator.ctx,
                s->stack,
                s->stack_cap * sizeof(AVLNode *),
                new_cap * sizeof(AVLNode *),
                _Alignof(AVLNode *));
            s->stack_cap = new_cap;
        }
        s->stack[s->stack_len++] = s->current;
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    AVLNode *node = s->stack[--s->stack_len];
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

static void avl_range_iter_drop(Iter *it)
{
    AVLRangeIterState *s = it->state;
    if (it->allocator.free)
    {
        if (s->stack)
            it->allocator.free(it->allocator.ctx, s->stack);
        if (s->hi)
            it->allocator.free(it->allocator.ctx, s->hi);
        it->allocator.free(it->allocator.ctx, s);
    }
}

static void avl_push_lo(AVLRangeIterState *s, AVLNode *node, const void *lo)
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
                size_t new_cap = s->stack_cap == 0 ? AVL_ITER_STACK_INIT_CAP
                                                   : s->stack_cap * 2;
                s->stack = s->allocator.realloc(
                    s->allocator.ctx,
                    s->stack,
                    s->stack_cap * sizeof(AVLNode *),
                    new_cap * sizeof(AVLNode *),
                    _Alignof(AVLNode *));
                s->stack_cap = new_cap;
            }
            s->stack[s->stack_len++] = node;
            node = node->left;
        }
    }
}

Iter avl_iter_range(const AVLTree *t, const void *lo, const void *hi)
{
    if (!t)
        return (Iter){0};
    AVLRangeIterState *s = t->allocator.alloc(
        t->allocator.ctx, sizeof *s, _Alignof(AVLRangeIterState));
    void *hi_copy = NULL;
    if (hi)
    {
        hi_copy = t->allocator.alloc(
            t->allocator.ctx, t->elem_size, _Alignof(max_align_t));
        memcpy(hi_copy, hi, t->elem_size);
    }
    *s = (AVLRangeIterState){
        .stack = NULL,
        .stack_len = 0,
        .stack_cap = 0,
        .current = NULL,
        .elem_size = t->elem_size,
        .allocator = t->allocator,
        .cmp = t->cmp,
        .hi = hi_copy};
    if (lo)
        avl_push_lo(s, t->root, lo);
    else
        s->current = t->root;
    return (Iter){
        .next = avl_range_iter_next,
        .drop = avl_range_iter_drop,
        .state = s,
        .elem_size = t->elem_size,
        .allocator = t->allocator};
}
