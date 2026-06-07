#include "seqc/avl.h"

#include <string.h>

typedef struct avl_node_t avl_node_t;
struct avl_node_t
{
    avl_node_t *left;
    avl_node_t *right;
    int height; /* 1-based; 0 used as sentinel for NULL */
                /* element data stored inline after the header */
};

struct avl_t
{
    avl_node_t *root;
    size_t len;
    size_t elem_size;
    compare_fn cmp;
    allocator_t allocator;
};

/* ---- node layout ------------------------------------------------------- */

static void *node_data(const avl_node_t *node)
{
    const size_t offset = (sizeof(avl_node_t) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size)
{
    const size_t offset = (sizeof(avl_node_t) + _Alignof(max_align_t) - 1)
                          & ~(_Alignof(max_align_t) - 1);
    return offset + elem_size;
}

static avl_node_t *make_node(const avl_t *t, const void *elem)
{
    avl_node_t *n = mem_alloc(
        t->allocator, node_alloc_size(t->elem_size), _Alignof(max_align_t));
    if (!n)
        return NULL;
    n->left = n->right = NULL;
    n->height = 1;
    memcpy(node_data(n), elem, t->elem_size);
    return n;
}

/* ---- AVL helpers ------------------------------------------------------- */

static int node_height(const avl_node_t *n)
{
    return n ? n->height : 0;
}

static int max2(int a, int b)
{
    return a > b ? a : b;
}

static void update_height(avl_node_t *n)
{
    n->height = 1 + max2(node_height(n->left), node_height(n->right));
}

static int balance_factor(const avl_node_t *n)
{
    return node_height(n->left) - node_height(n->right);
}

/* ---- rotations --------------------------------------------------------- */

/*       y                 x
 *      / \              /   \
 *     x   C    -->    A       y
 *    / \                     / \
 *   A   B                   B   C   */
static avl_node_t *rotate_right(avl_node_t *y)
{
    avl_node_t *x = y->left;
    avl_node_t *b = x->right;
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
static avl_node_t *rotate_left(avl_node_t *x)
{
    avl_node_t *y = x->right;
    avl_node_t *b = y->left;
    y->left = x;
    x->right = b;
    update_height(x);
    update_height(y);
    return y;
}

static avl_node_t *rebalance(avl_node_t *n)
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

avl_t *avl_create(size_t elem_size, compare_fn cmp, allocator_t allocator)
{
    avl_t *t = mem_alloc(allocator, sizeof(avl_t), _Alignof(avl_t));
    if (!t)
        return NULL;
    *t = (avl_t){.root = NULL,
                   .len = 0,
                   .elem_size = elem_size,
                   .cmp = cmp,
                   .allocator = allocator};
    return t;
}

/* --- insert ------------------------------------------------------------- */

static avl_node_t *do_insert(
    avl_t *t, avl_node_t *node, const void *elem, bool *inserted, bool *oom)
{
    if (!node)
    {
        avl_node_t *n = make_node(t, elem);
        if (!n)
        {
            *oom = true;
            return NULL;
        }
        *inserted = true;
        return n;
    }
    int c = t->cmp(elem, node_data(node));
    if (c < 0)
    {
        avl_node_t *new_left = do_insert(t, node->left, elem, inserted, oom);
        if (*oom)
            return node; /* preserve existing tree on OOM */
        node->left = new_left;
    }
    else if (c > 0)
    {
        avl_node_t *new_right = do_insert(t, node->right, elem, inserted, oom);
        if (*oom)
            return node;
        node->right = new_right;
    }
    else
    {
        *inserted = false;
        return node; /* duplicate — no rotation needed */
    }
    return rebalance(node);
}

seqc_status_t avl_insert(avl_t *t, const void *elem)
{
    if (!t || !elem)
        return SEQC_INVALID;
    bool inserted = false;
    bool oom = false;
    avl_node_t *new_root = do_insert(t, t->root, elem, &inserted, &oom);
    if (oom)
        return SEQC_OOM;
    if (new_root)
        t->root = new_root;
    if (inserted)
        t->len++;
    return inserted ? SEQC_OK : SEQC_DUPLICATE;
}

/* --- contains ----------------------------------------------------------- */

bool avl_contains(const avl_t *t, const void *elem)
{
    if (!t || !elem)
        return false;
    avl_node_t *cur = t->root;
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

static avl_node_t *min_node(avl_node_t *n)
{
    while (n->left)
        n = n->left;
    return n;
}

static avl_node_t *do_remove(
    avl_t *t, avl_node_t *node, const void *elem, bool *removed)
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
            avl_node_t *r = node->right;
            mem_free(t->allocator, node, node_alloc_size(t->elem_size));
            return r;
        }
        if (!node->right)
        {
            avl_node_t *l = node->left;
            mem_free(t->allocator, node, node_alloc_size(t->elem_size));
            return l;
        }
        /* two children: overwrite with in-order successor then delete it below
         */
        avl_node_t *succ = min_node(node->right);
        memcpy(node_data(node), node_data(succ), t->elem_size);
        bool dummy = false;
        node->right = do_remove(t, node->right, node_data(node), &dummy);
    }
    return rebalance(node);
}

seqc_status_t avl_remove(avl_t *t, const void *elem)
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

void *avl_min(const avl_t *t)
{
    if (!t || !t->root)
        return NULL;
    avl_node_t *cur = t->root;
    while (cur->left)
        cur = cur->left;
    return node_data(cur);
}

void *avl_max(const avl_t *t)
{
    if (!t || !t->root)
        return NULL;
    avl_node_t *cur = t->root;
    while (cur->right)
        cur = cur->right;
    return node_data(cur);
}

/* --- misc --------------------------------------------------------------- */

size_t avl_len(const avl_t *t)
{
    return t ? t->len : 0;
}
int avl_height(const avl_t *t)
{
    return t ? node_height(t->root) : 0;
}

static void free_subtree(avl_t *t, avl_node_t *node)
{
    if (!node)
        return;
    free_subtree(t, node->left);
    free_subtree(t, node->right);
    mem_free(t->allocator, node, node_alloc_size(t->elem_size));
}

void avl_free(avl_t *t)
{
    if (!t)
        return;
    free_subtree(t, t->root);
    allocator_t al = t->allocator;
    mem_free(al, t, sizeof(avl_t));
}

void avl_clear(avl_t *t)
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
    avl_node_t **stack;
    size_t stack_len;
    size_t stack_cap;
    avl_node_t *current;
    size_t elem_size;
    allocator_t allocator;
} avl_iter_state_t;

/* Push node onto a growable traversal stack, doubling capacity on demand.
 * Returns false on OOM, leaving the existing stack intact for the drop. */
static bool avl_stack_push(
    avl_node_t ***stack, size_t *len, size_t *cap, allocator_t al, avl_node_t *node)
{
    if (*len == *cap)
    {
        size_t new_cap = *cap == 0 ? AVL_ITER_STACK_INIT_CAP : *cap * 2;
        avl_node_t **grown = mem_realloc(al, *stack, *cap * sizeof(avl_node_t *),
                                      new_cap * sizeof(avl_node_t *),
                                      _Alignof(avl_node_t *));
        if (!grown)
            return false;
        *stack = grown;
        *cap = new_cap;
    }
    (*stack)[(*len)++] = node;
    return true;
}

static bool avl_iter_next(iter_t *it, void *out)
{
    avl_iter_state_t *s = it->state;
    while (s->current)
    {
        if (!avl_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                            s->allocator, s->current))
            return false; /* OOM: end iteration; drop frees the stack */
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    avl_node_t *node = s->stack[--s->stack_len];
    memcpy(out, node_data(node), s->elem_size);
    s->current = node->right;
    return true;
}

static void avl_iter_drop(iter_t *it)
{
    avl_iter_state_t *s = it->state;
    if (s->stack)
        mem_free(it->allocator, s->stack, s->stack_cap * sizeof(avl_node_t *));
    mem_free(it->allocator, s, sizeof(avl_iter_state_t));
}

iter_t avl_iter(const avl_t *t)
{
    if (!t)
        return (iter_t){0};
    avl_iter_state_t *s =
        mem_alloc(t->allocator, sizeof *s, _Alignof(avl_iter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (avl_iter_state_t){.stack = NULL,
                        .stack_len = 0,
                        .stack_cap = 0,
                        .current = t->root,
                        .elem_size = t->elem_size,
                        .allocator = t->allocator};
    return (iter_t){.next = avl_iter_next,
                  .drop = avl_iter_drop,
                  .state = s,
                  .elem_size = t->elem_size,
                  .allocator = t->allocator};
}

static bool avl_iter_rev_next(iter_t *it, void *out)
{
    avl_iter_state_t *s = it->state;
    while (s->current)
    {
        if (!avl_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                            s->allocator, s->current))
            return false; /* OOM: end iteration; drop frees the stack */
        s->current = s->current->right;
    }
    if (s->stack_len == 0)
        return false;
    avl_node_t *node = s->stack[--s->stack_len];
    memcpy(out, node_data(node), s->elem_size);
    s->current = node->left;
    return true;
}

iter_t avl_iter_rev(const avl_t *t)
{
    if (!t)
        return (iter_t){0};
    avl_iter_state_t *s =
        mem_alloc(t->allocator, sizeof *s, _Alignof(avl_iter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (avl_iter_state_t){.stack = NULL,
                        .stack_len = 0,
                        .stack_cap = 0,
                        .current = t->root,
                        .elem_size = t->elem_size,
                        .allocator = t->allocator};
    return (iter_t){.next = avl_iter_rev_next,
                  .drop = avl_iter_drop,
                  .state = s,
                  .elem_size = t->elem_size,
                  .allocator = t->allocator};
}

/* ---- range iterator ---------------------------------------------------- */

typedef struct
{
    avl_node_t **stack;
    size_t stack_len;
    size_t stack_cap;
    avl_node_t *current;
    size_t elem_size;
    allocator_t allocator;
    compare_fn cmp;
    void *hi; /* NULL means no upper bound; owned allocation */
} avl_range_iter_state_t;

static bool avl_range_iter_next(iter_t *it, void *out)
{
    avl_range_iter_state_t *s = it->state;
    while (s->current)
    {
        if (!avl_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                            s->allocator, s->current))
            return false; /* OOM: end iteration; drop frees the stack */
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    avl_node_t *node = s->stack[--s->stack_len];
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

static void avl_range_iter_drop(iter_t *it)
{
    avl_range_iter_state_t *s = it->state;
    if (s->stack)
        mem_free(it->allocator, s->stack, s->stack_cap * sizeof(avl_node_t *));
    if (s->hi)
        mem_free(it->allocator, s->hi, s->elem_size);
    mem_free(it->allocator, s, sizeof(avl_range_iter_state_t));
}

static void avl_push_lo(avl_range_iter_state_t *s, avl_node_t *node, const void *lo)
{
    while (node)
    {
        if (s->cmp(node_data(node), lo) < 0)
        {
            node = node->right;
        }
        else
        {
            if (!avl_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                                s->allocator, node))
                return; /* OOM: stop priming; iterator yields what it has */
            node = node->left;
        }
    }
}

iter_t avl_iter_range(const avl_t *t, const void *lo, const void *hi)
{
    if (!t)
        return (iter_t){0};
    avl_range_iter_state_t *s =
        mem_alloc(t->allocator, sizeof *s, _Alignof(avl_range_iter_state_t));
    if (!s)
        return (iter_t){0};
    void *hi_copy = NULL;
    if (hi)
    {
        hi_copy = mem_alloc(t->allocator, t->elem_size, _Alignof(max_align_t));
        if (!hi_copy)
        {
            mem_free(t->allocator, s, sizeof(avl_range_iter_state_t));
            return (iter_t){0};
        }
        memcpy(hi_copy, hi, t->elem_size);
    }
    *s = (avl_range_iter_state_t){.stack = NULL,
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
    return (iter_t){.next = avl_range_iter_next,
                  .drop = avl_range_iter_drop,
                  .state = s,
                  .elem_size = t->elem_size,
                  .allocator = t->allocator};
}
