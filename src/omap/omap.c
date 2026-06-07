#include "seqc/omap.h"

#include <string.h>

typedef struct omap_node_t omap_node_t;
struct omap_node_t
{
    omap_node_t *left;
    omap_node_t *right;
    int height;
    /* key data then value data stored inline after the header */
};

struct omap_t
{
    omap_node_t *root;
    size_t len;
    size_t key_size;
    size_t val_size;
    compare_fn cmp;
    allocator_t allocator;
};

/* ---- node layout -------------------------------------------------------
 *
 *  [ omap_node_t header | ... pad ... | key (key_size) | ... pad ... | value
 * (val_size) ]
 *
 * Both key and value are aligned to max_align_t.
 * ----------------------------------------------------------------------- */

static size_t key_off(void)
{
    return (sizeof(omap_node_t) + _Alignof(max_align_t) - 1)
           & ~(_Alignof(max_align_t) - 1);
}

static size_t val_off(size_t key_size)
{
    size_t after_key = key_off() + key_size;
    return (after_key + _Alignof(max_align_t) - 1)
           & ~(_Alignof(max_align_t) - 1);
}

static size_t node_sz(size_t key_size, size_t val_size)
{
    return val_off(key_size) + val_size;
}

static void *node_key(const omap_node_t *n)
{
    return (char *)n + key_off();
}

static void *node_val(const omap_node_t *n, size_t key_size)
{
    return (char *)n + val_off(key_size);
}

static omap_node_t *make_node(const omap_t *m, const void *key, const void *value)
{
    omap_node_t *n = mem_alloc(
        m->allocator,

        node_sz(m->key_size, m->val_size),
        _Alignof(max_align_t));
    if (!n)
        return NULL;
    n->left = n->right = NULL;
    n->height = 1;
    memcpy(node_key(n), key, m->key_size);
    memcpy(node_val(n, m->key_size), value, m->val_size);
    return n;
}

/* ---- AVL balance ------------------------------------------------------- */

static int node_height(const omap_node_t *n)
{
    return n ? n->height : 0;
}
static int max2(int a, int b)
{
    return a > b ? a : b;
}

static void update_height(omap_node_t *n)
{
    n->height = 1 + max2(node_height(n->left), node_height(n->right));
}

static int balance_factor(const omap_node_t *n)
{
    return node_height(n->left) - node_height(n->right);
}

static omap_node_t *rotate_right(omap_node_t *y)
{
    omap_node_t *x = y->left;
    y->left = x->right;
    x->right = y;
    update_height(y);
    update_height(x);
    return x;
}

static omap_node_t *rotate_left(omap_node_t *x)
{
    omap_node_t *y = x->right;
    x->right = y->left;
    y->left = x;
    update_height(x);
    update_height(y);
    return y;
}

static omap_node_t *rebalance(omap_node_t *n)
{
    update_height(n);
    int bf = balance_factor(n);
    if (bf > 1 && balance_factor(n->left) >= 0)
        return rotate_right(n);
    if (bf > 1 && balance_factor(n->left) < 0)
    {
        n->left = rotate_left(n->left);
        return rotate_right(n);
    }
    if (bf < -1 && balance_factor(n->right) <= 0)
        return rotate_left(n);
    if (bf < -1 && balance_factor(n->right) > 0)
    {
        n->right = rotate_right(n->right);
        return rotate_left(n);
    }
    return n;
}

/* ---- public API -------------------------------------------------------- */

omap_t *omap_create(
    size_t key_size, size_t val_size, compare_fn cmp, allocator_t allocator)
{
    omap_t *m = mem_alloc(allocator, sizeof(omap_t), _Alignof(omap_t));
    if (!m)
        return NULL;
    *m = (omap_t){.root = NULL,
                .len = 0,
                .key_size = key_size,
                .val_size = val_size,
                .cmp = cmp,
                .allocator = allocator};
    return m;
}

/* ---- set (insert or update) ------------------------------------------- */

static omap_node_t *do_set(
    omap_t *m,
    omap_node_t *node,
    const void *key,
    const void *value,
    bool *inserted,
    bool *oom)
{
    if (!node)
    {
        omap_node_t *n = make_node(m, key, value);
        if (!n)
        {
            *oom = true;
            return NULL;
        }
        *inserted = true;
        return n;
    }
    int c = m->cmp(key, node_key(node));
    if (c < 0)
    {
        omap_node_t *new_left = do_set(m, node->left, key, value, inserted, oom);
        if (*oom)
            return node;
        node->left = new_left;
    }
    else if (c > 0)
    {
        omap_node_t *new_right = do_set(m, node->right, key, value, inserted, oom);
        if (*oom)
            return node;
        node->right = new_right;
    }
    else
    {
        /* key exists — update value in place, no structural change */
        memcpy(node_val(node, m->key_size), value, m->val_size);
        *inserted = false;
        return node;
    }
    return rebalance(node);
}

seqc_status_t omap_set(omap_t *m, const void *key, const void *value)
{
    if (!m || !key || !value)
        return SEQC_INVALID;
    bool inserted = false;
    bool oom = false;
    omap_node_t *new_root = do_set(m, m->root, key, value, &inserted, &oom);
    if (oom)
        return SEQC_OOM;
    if (new_root)
        m->root = new_root;
    if (inserted)
        m->len++;
    return SEQC_OK;
}

/* ---- get --------------------------------------------------------------- */

seqc_status_t omap_get(const omap_t *m, const void *key, void *out)
{
    if (!m || !key)
        return SEQC_INVALID;
    omap_node_t *cur = m->root;
    while (cur)
    {
        int c = m->cmp(key, node_key(cur));
        if (c < 0)
            cur = cur->left;
        else if (c > 0)
            cur = cur->right;
        else
        {
            if (out)
                memcpy(out, node_val(cur, m->key_size), m->val_size);
            return SEQC_OK;
        }
    }
    return SEQC_NOT_FOUND;
}

bool omap_contains(const omap_t *m, const void *key)
{
    return omap_get(m, key, NULL) == SEQC_OK;
}

/* ---- remove ------------------------------------------------------------ */

static omap_node_t *min_node(omap_node_t *n)
{
    while (n->left)
        n = n->left;
    return n;
}

static omap_node_t *do_remove(omap_t *m, omap_node_t *node, const void *key, bool *removed)
{
    if (!node)
    {
        *removed = false;
        return NULL;
    }
    int c = m->cmp(key, node_key(node));
    if (c < 0)
    {
        node->left = do_remove(m, node->left, key, removed);
    }
    else if (c > 0)
    {
        node->right = do_remove(m, node->right, key, removed);
    }
    else
    {
        *removed = true;
        if (!node->left)
        {
            omap_node_t *r = node->right;
            mem_free(m->allocator, node, node_sz(m->key_size, m->val_size));
            return r;
        }
        if (!node->right)
        {
            omap_node_t *l = node->left;
            mem_free(m->allocator, node, node_sz(m->key_size, m->val_size));
            return l;
        }
        /* two children: copy in-order successor's key+value here, delete it
         * below
         */
        omap_node_t *succ = min_node(node->right);
        memcpy(node_key(node), node_key(succ), m->key_size);
        memcpy(
            node_val(node, m->key_size),
            node_val(succ, m->key_size),
            m->val_size);
        bool dummy = false;
        node->right = do_remove(m, node->right, node_key(node), &dummy);
    }
    return rebalance(node);
}

seqc_status_t omap_remove(omap_t *m, const void *key)
{
    if (!m || !key)
        return SEQC_INVALID;
    bool removed = false;
    m->root = do_remove(m, m->root, key, &removed);
    if (removed)
        m->len--;
    return removed ? SEQC_OK : SEQC_NOT_FOUND;
}

/* ---- min / max --------------------------------------------------------- */

seqc_status_t omap_min_key(const omap_t *m, void *out)
{
    if (!m || !m->root)
        return SEQC_NOT_FOUND;
    omap_node_t *cur = m->root;
    while (cur->left)
        cur = cur->left;
    if (out)
        memcpy(out, node_key(cur), m->key_size);
    return SEQC_OK;
}

seqc_status_t omap_max_key(const omap_t *m, void *out)
{
    if (!m || !m->root)
        return SEQC_NOT_FOUND;
    omap_node_t *cur = m->root;
    while (cur->right)
        cur = cur->right;
    if (out)
        memcpy(out, node_key(cur), m->key_size);
    return SEQC_OK;
}

seqc_status_t omap_min_entry(const omap_t *m, void *key_out, void *val_out)
{
    if (!m || !m->root)
        return SEQC_NOT_FOUND;
    omap_node_t *cur = m->root;
    while (cur->left)
        cur = cur->left;
    if (key_out)
        memcpy(key_out, node_key(cur), m->key_size);
    if (val_out)
        memcpy(val_out, node_val(cur, m->key_size), m->val_size);
    return SEQC_OK;
}

seqc_status_t omap_max_entry(const omap_t *m, void *key_out, void *val_out)
{
    if (!m || !m->root)
        return SEQC_NOT_FOUND;
    omap_node_t *cur = m->root;
    while (cur->right)
        cur = cur->right;
    if (key_out)
        memcpy(key_out, node_key(cur), m->key_size);
    if (val_out)
        memcpy(val_out, node_val(cur, m->key_size), m->val_size);
    return SEQC_OK;
}

size_t omap_len(const omap_t *m)
{
    return m ? m->len : 0;
}
int omap_height(const omap_t *m)
{
    return m ? node_height(m->root) : 0;
}

static void free_subtree(omap_t *m, omap_node_t *node)
{
    if (!node)
        return;
    free_subtree(m, node->left);
    free_subtree(m, node->right);
    mem_free(m->allocator, node, node_sz(m->key_size, m->val_size));
}

void omap_free(omap_t *m)
{
    if (!m)
        return;
    free_subtree(m, m->root);
    allocator_t al = m->allocator;
    mem_free(al, m, sizeof(omap_t));
}

void omap_clear(omap_t *m)
{
    if (!m)
        return;
    free_subtree(m, m->root);
    m->root = NULL;
    m->len = 0;
}

/* ---- iter: iterative in-order, yields omap_entry_t ----------------------- */

#define OMAP_ITER_STACK_INIT_CAP 16

typedef struct
{
    omap_node_t **stack;
    size_t stack_len;
    size_t stack_cap;
    omap_node_t *current;
    size_t key_size;
    allocator_t allocator;
} omap_iter_state_t;

/* Push node onto a growable traversal stack, doubling capacity on demand.
 * Returns false on OOM, leaving the existing stack intact for the drop. */
static bool omap_stack_push(
    omap_node_t ***stack, size_t *len, size_t *cap, allocator_t al, omap_node_t *node)
{
    if (*len == *cap)
    {
        size_t new_cap = *cap == 0 ? OMAP_ITER_STACK_INIT_CAP : *cap * 2;
        omap_node_t **grown =
            mem_realloc(al, *stack, *cap * sizeof(omap_node_t *),
                        new_cap * sizeof(omap_node_t *), _Alignof(omap_node_t *));
        if (!grown)
            return false;
        *stack = grown;
        *cap = new_cap;
    }
    (*stack)[(*len)++] = node;
    return true;
}

static bool omap_iter_next(iter_t *it, void *out)
{
    omap_iter_state_t *s = it->state;
    while (s->current)
    {
        if (!omap_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                             s->allocator, s->current))
            return false; /* OOM: end iteration; drop frees the stack */
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    omap_node_t *node = s->stack[--s->stack_len];
    /* write an omap_entry_t with pointers into the live node */
    omap_entry_t entry = {node_key(node), node_val(node, s->key_size)};
    memcpy(out, &entry, sizeof(omap_entry_t));
    s->current = node->right;
    return true;
}

static void omap_iter_drop(iter_t *it)
{
    omap_iter_state_t *s = it->state;
    if (s->stack)
        mem_free(it->allocator, s->stack, s->stack_cap * sizeof(omap_node_t *));
    mem_free(it->allocator, s, sizeof(omap_iter_state_t));
}

iter_t omap_iter(const omap_t *m)
{
    if (!m)
        return (iter_t){0};
    omap_iter_state_t *s =
        mem_alloc(m->allocator, sizeof *s, _Alignof(omap_iter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (omap_iter_state_t){.stack = NULL,
                         .stack_len = 0,
                         .stack_cap = 0,
                         .current = m->root,
                         .key_size = m->key_size,
                         .allocator = m->allocator};
    return (iter_t){.next = omap_iter_next,
                  .drop = omap_iter_drop,
                  .state = s,
                  .elem_size = sizeof(omap_entry_t),
                  .allocator = m->allocator};
}

static bool omap_iter_rev_next(iter_t *it, void *out)
{
    omap_iter_state_t *s = it->state;
    while (s->current)
    {
        if (!omap_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                             s->allocator, s->current))
            return false; /* OOM: end iteration; drop frees the stack */
        s->current = s->current->right;
    }
    if (s->stack_len == 0)
        return false;
    omap_node_t *node = s->stack[--s->stack_len];
    omap_entry_t entry = {node_key(node), node_val(node, s->key_size)};
    memcpy(out, &entry, sizeof(omap_entry_t));
    s->current = node->left;
    return true;
}

iter_t omap_iter_rev(const omap_t *m)
{
    if (!m)
        return (iter_t){0};
    omap_iter_state_t *s =
        mem_alloc(m->allocator, sizeof *s, _Alignof(omap_iter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (omap_iter_state_t){.stack = NULL,
                         .stack_len = 0,
                         .stack_cap = 0,
                         .current = m->root,
                         .key_size = m->key_size,
                         .allocator = m->allocator};
    return (iter_t){.next = omap_iter_rev_next,
                  .drop = omap_iter_drop,
                  .state = s,
                  .elem_size = sizeof(omap_entry_t),
                  .allocator = m->allocator};
}

/* ---- range iterator ---------------------------------------------------- */

typedef struct
{
    omap_node_t **stack;
    size_t stack_len;
    size_t stack_cap;
    omap_node_t *current;
    size_t key_size;
    allocator_t allocator;
    compare_fn cmp;
    void *hi_key; /* NULL means no upper bound; owned allocation */
} omap_range_iter_state_t;

static bool omap_range_iter_next(iter_t *it, void *out)
{
    omap_range_iter_state_t *s = it->state;
    while (s->current)
    {
        if (!omap_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                             s->allocator, s->current))
            return false; /* OOM: end iteration; drop frees the stack */
        s->current = s->current->left;
    }
    if (s->stack_len == 0)
        return false;
    omap_node_t *node = s->stack[--s->stack_len];
    void *key = node_key(node);
    if (s->hi_key && s->cmp(key, s->hi_key) > 0)
    {
        s->stack_len = 0;
        s->current = NULL;
        return false;
    }
    omap_entry_t entry = {key, node_val(node, s->key_size)};
    memcpy(out, &entry, sizeof(omap_entry_t));
    s->current = node->right;
    return true;
}

static void omap_range_iter_drop(iter_t *it)
{
    omap_range_iter_state_t *s = it->state;
    if (s->stack)
        mem_free(it->allocator, s->stack, s->stack_cap * sizeof(omap_node_t *));
    if (s->hi_key)
        mem_free(it->allocator, s->hi_key, s->key_size);
    mem_free(it->allocator, s, sizeof(omap_range_iter_state_t));
}

static void omap_push_lo(
    omap_range_iter_state_t *s, omap_node_t *node, const void *lo_key)
{
    while (node)
    {
        if (s->cmp(node_key(node), lo_key) < 0)
        {
            node = node->right;
        }
        else
        {
            if (!omap_stack_push(&s->stack, &s->stack_len, &s->stack_cap,
                                 s->allocator, node))
                return; /* OOM: stop priming; iterator yields what it has */
            node = node->left;
        }
    }
}

iter_t omap_iter_range(const omap_t *m, const void *lo_key, const void *hi_key)
{
    if (!m)
        return (iter_t){0};
    omap_range_iter_state_t *s =
        mem_alloc(m->allocator, sizeof *s, _Alignof(omap_range_iter_state_t));
    if (!s)
        return (iter_t){0};
    void *hi_copy = NULL;
    if (hi_key)
    {
        hi_copy = mem_alloc(m->allocator, m->key_size, _Alignof(max_align_t));
        if (!hi_copy)
        {
            mem_free(m->allocator, s, sizeof(omap_range_iter_state_t));
            return (iter_t){0};
        }
        memcpy(hi_copy, hi_key, m->key_size);
    }
    *s = (omap_range_iter_state_t){.stack = NULL,
                              .stack_len = 0,
                              .stack_cap = 0,
                              .current = NULL,
                              .key_size = m->key_size,
                              .allocator = m->allocator,
                              .cmp = m->cmp,
                              .hi_key = hi_copy};
    if (lo_key)
        omap_push_lo(s, m->root, lo_key);
    else
        s->current = m->root;
    return (iter_t){.next = omap_range_iter_next,
                  .drop = omap_range_iter_drop,
                  .state = s,
                  .elem_size = sizeof(omap_entry_t),
                  .allocator = m->allocator};
}
