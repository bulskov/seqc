#include "omap.h"

#include <string.h>

/* ---- node layout -------------------------------------------------------
 *
 *  [ OMNode header | ... pad ... | key (key_size) | ... pad ... | value
 * (val_size) ]
 *
 * Both key and value are aligned to max_align_t.
 * ----------------------------------------------------------------------- */

static size_t key_off(void) {
  return (sizeof(OMNode) + _Alignof(max_align_t) - 1) &
         ~(_Alignof(max_align_t) - 1);
}

static size_t val_off(size_t key_size) {
  size_t after_key = key_off() + key_size;
  return (after_key + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1);
}

static size_t node_sz(size_t key_size, size_t val_size) {
  return val_off(key_size) + val_size;
}

static void *node_key(const OMNode *n) { return (char *)n + key_off(); }

static void *node_val(const OMNode *n, size_t key_size) {
  return (char *)n + val_off(key_size);
}

static OMNode *make_node(const OMap *m, const void *key, const void *value) {
  OMNode *n =
      m->allocator.alloc(m->allocator.ctx, node_sz(m->key_size, m->val_size),
                         _Alignof(max_align_t));
  n->left = n->right = NULL;
  n->height = 1;
  memcpy(node_key(n), key, m->key_size);
  memcpy(node_val(n, m->key_size), value, m->val_size);
  return n;
}

/* ---- AVL balance ------------------------------------------------------- */

static int node_height(const OMNode *n) { return n ? n->height : 0; }
static int max2(int a, int b) { return a > b ? a : b; }

static void update_height(OMNode *n) {
  n->height = 1 + max2(node_height(n->left), node_height(n->right));
}

static int balance_factor(const OMNode *n) {
  return node_height(n->left) - node_height(n->right);
}

static OMNode *rotate_right(OMNode *y) {
  OMNode *x = y->left;
  y->left = x->right;
  x->right = y;
  update_height(y);
  update_height(x);
  return x;
}

static OMNode *rotate_left(OMNode *x) {
  OMNode *y = x->right;
  x->right = y->left;
  y->left = x;
  update_height(x);
  update_height(y);
  return y;
}

static OMNode *rebalance(OMNode *n) {
  update_height(n);
  int bf = balance_factor(n);
  if (bf > 1 && balance_factor(n->left) >= 0)
    return rotate_right(n);
  if (bf > 1 && balance_factor(n->left) < 0) {
    n->left = rotate_left(n->left);
    return rotate_right(n);
  }
  if (bf < -1 && balance_factor(n->right) <= 0)
    return rotate_left(n);
  if (bf < -1 && balance_factor(n->right) > 0) {
    n->right = rotate_right(n->right);
    return rotate_left(n);
  }
  return n;
}

/* ---- public API -------------------------------------------------------- */

OMap omap_create(size_t key_size, size_t val_size, compare_fn cmp,
                 Allocator allocator) {
  return (OMap){.root = NULL,
                .len = 0,
                .key_size = key_size,
                .val_size = val_size,
                .cmp = cmp,
                .allocator = allocator};
}

/* ---- set (insert or update) ------------------------------------------- */

static OMNode *do_set(OMap *m, OMNode *node, const void *key, const void *value,
                      int *inserted) {
  if (!node) {
    *inserted = 1;
    return make_node(m, key, value);
  }
  int c = m->cmp(key, node_key(node));
  if (c < 0) {
    node->left = do_set(m, node->left, key, value, inserted);
  } else if (c > 0) {
    node->right = do_set(m, node->right, key, value, inserted);
  } else {
    /* key exists — update value in place, no structural change */
    memcpy(node_val(node, m->key_size), value, m->val_size);
    *inserted = 0;
    return node;
  }
  return rebalance(node);
}

int omap_set(OMap *m, const void *key, const void *value) {
  if (!m || !key || !value)
    return 0;
  int inserted = 0;
  m->root = do_set(m, m->root, key, value, &inserted);
  if (inserted)
    m->len++;
  return inserted;
}

/* ---- get --------------------------------------------------------------- */

void *omap_get(const OMap *m, const void *key) {
  if (!m || !key)
    return NULL;
  OMNode *cur = m->root;
  while (cur) {
    int c = m->cmp(key, node_key(cur));
    if (c < 0)
      cur = cur->left;
    else if (c > 0)
      cur = cur->right;
    else
      return node_val(cur, m->key_size);
  }
  return NULL;
}

int omap_contains(const OMap *m, const void *key) {
  return omap_get(m, key) != NULL;
}

/* ---- remove ------------------------------------------------------------ */

static OMNode *min_node(OMNode *n) {
  while (n->left)
    n = n->left;
  return n;
}

static OMNode *do_remove(OMap *m, OMNode *node, const void *key, int *removed) {
  if (!node) {
    *removed = 0;
    return NULL;
  }
  int c = m->cmp(key, node_key(node));
  if (c < 0) {
    node->left = do_remove(m, node->left, key, removed);
  } else if (c > 0) {
    node->right = do_remove(m, node->right, key, removed);
  } else {
    *removed = 1;
    if (!node->left) {
      OMNode *r = node->right;
      if (m->allocator.free)
        m->allocator.free(m->allocator.ctx, node);
      return r;
    }
    if (!node->right) {
      OMNode *l = node->left;
      if (m->allocator.free)
        m->allocator.free(m->allocator.ctx, node);
      return l;
    }
    /* two children: copy in-order successor's key+value here, delete it below
     */
    OMNode *succ = min_node(node->right);
    memcpy(node_key(node), node_key(succ), m->key_size);
    memcpy(node_val(node, m->key_size), node_val(succ, m->key_size),
           m->val_size);
    int dummy = 0;
    node->right = do_remove(m, node->right, node_key(node), &dummy);
  }
  return rebalance(node);
}

int omap_remove(OMap *m, const void *key) {
  if (!m || !key)
    return 0;
  int removed = 0;
  m->root = do_remove(m, m->root, key, &removed);
  if (removed)
    m->len--;
  return removed;
}

/* ---- min / max --------------------------------------------------------- */

void *omap_min_key(const OMap *m) {
  if (!m || !m->root)
    return NULL;
  OMNode *cur = m->root;
  while (cur->left)
    cur = cur->left;
  return node_key(cur);
}

void *omap_max_key(const OMap *m) {
  if (!m || !m->root)
    return NULL;
  OMNode *cur = m->root;
  while (cur->right)
    cur = cur->right;
  return node_key(cur);
}

size_t omap_len(const OMap *m) { return m ? m->len : 0; }
int omap_height(const OMap *m) { return m ? node_height(m->root) : 0; }

static void free_subtree(OMap *m, OMNode *node) {
  if (!node)
    return;
  free_subtree(m, node->left);
  free_subtree(m, node->right);
  if (m->allocator.free)
    m->allocator.free(m->allocator.ctx, node);
}

void omap_free(OMap *m) {
  if (!m)
    return;
  free_subtree(m, m->root);
  m->root = NULL;
  m->len = 0;
}

/* ---- iter: iterative in-order, yields OMapEntry ----------------------- */

#define OMAP_ITER_STACK_INIT_CAP 16

typedef struct {
  OMNode **stack;
  size_t stack_len;
  size_t stack_cap;
  OMNode *current;
  size_t key_size;
  Allocator allocator;
} OMapIterState;

static int omap_iter_next(Iter *it, void *out) {
  OMapIterState *s = it->state;
  while (s->current) {
    if (s->stack_len == s->stack_cap) {
      size_t new_cap =
          s->stack_cap == 0 ? OMAP_ITER_STACK_INIT_CAP : s->stack_cap * 2;
      s->stack = s->allocator.realloc(
          s->allocator.ctx, s->stack, s->stack_cap * sizeof(OMNode *),
          new_cap * sizeof(OMNode *), _Alignof(OMNode *));
      s->stack_cap = new_cap;
    }
    s->stack[s->stack_len++] = s->current;
    s->current = s->current->left;
  }
  if (s->stack_len == 0)
    return 0;
  OMNode *node = s->stack[--s->stack_len];
  /* write an OMapEntry with pointers into the live node */
  OMapEntry entry = {node_key(node), node_val(node, s->key_size)};
  memcpy(out, &entry, sizeof(OMapEntry));
  s->current = node->right;
  return 1;
}

static void omap_iter_drop(Iter *it) {
  OMapIterState *s = it->state;
  if (it->allocator.free) {
    if (s->stack)
      it->allocator.free(it->allocator.ctx, s->stack);
    it->allocator.free(it->allocator.ctx, s);
  }
}

Iter omap_iter(const OMap *m) {
  if (!m)
    return (Iter){0};
  OMapIterState *s =
      m->allocator.alloc(m->allocator.ctx, sizeof *s, _Alignof(OMapIterState));
  *s = (OMapIterState){.stack = NULL,
                       .stack_len = 0,
                       .stack_cap = 0,
                       .current = m->root,
                       .key_size = m->key_size,
                       .allocator = m->allocator};
  return (Iter){.next = omap_iter_next,
                .drop = omap_iter_drop,
                .state = s,
                .elem_size = sizeof(OMapEntry),
                .allocator = m->allocator};
}

static int omap_iter_rev_next(Iter *it, void *out) {
  OMapIterState *s = it->state;
  while (s->current) {
    if (s->stack_len == s->stack_cap) {
      size_t new_cap =
          s->stack_cap == 0 ? OMAP_ITER_STACK_INIT_CAP : s->stack_cap * 2;
      s->stack = s->allocator.realloc(
          s->allocator.ctx, s->stack, s->stack_cap * sizeof(OMNode *),
          new_cap * sizeof(OMNode *), _Alignof(OMNode *));
      s->stack_cap = new_cap;
    }
    s->stack[s->stack_len++] = s->current;
    s->current = s->current->right;
  }
  if (s->stack_len == 0)
    return 0;
  OMNode *node = s->stack[--s->stack_len];
  OMapEntry entry = {node_key(node), node_val(node, s->key_size)};
  memcpy(out, &entry, sizeof(OMapEntry));
  s->current = node->left;
  return 1;
}

Iter omap_iter_rev(const OMap *m) {
  if (!m)
    return (Iter){0};
  OMapIterState *s =
      m->allocator.alloc(m->allocator.ctx, sizeof *s, _Alignof(OMapIterState));
  *s = (OMapIterState){.stack = NULL,
                       .stack_len = 0,
                       .stack_cap = 0,
                       .current = m->root,
                       .key_size = m->key_size,
                       .allocator = m->allocator};
  return (Iter){.next = omap_iter_rev_next,
                .drop = omap_iter_drop,
                .state = s,
                .elem_size = sizeof(OMapEntry),
                .allocator = m->allocator};
}

/* ---- range iterator ---------------------------------------------------- */

typedef struct {
  OMNode **stack;
  size_t stack_len;
  size_t stack_cap;
  OMNode *current;
  size_t key_size;
  Allocator allocator;
  compare_fn cmp;
  void *hi_key; /* NULL means no upper bound; owned allocation */
} OMapRangeIterState;

static int omap_range_iter_next(Iter *it, void *out) {
  OMapRangeIterState *s = it->state;
  while (s->current) {
    if (s->stack_len == s->stack_cap) {
      size_t new_cap =
          s->stack_cap == 0 ? OMAP_ITER_STACK_INIT_CAP : s->stack_cap * 2;
      s->stack = s->allocator.realloc(
          s->allocator.ctx, s->stack, s->stack_cap * sizeof(OMNode *),
          new_cap * sizeof(OMNode *), _Alignof(OMNode *));
      s->stack_cap = new_cap;
    }
    s->stack[s->stack_len++] = s->current;
    s->current = s->current->left;
  }
  if (s->stack_len == 0)
    return 0;
  OMNode *node = s->stack[--s->stack_len];
  void *key = node_key(node);
  if (s->hi_key && s->cmp(key, s->hi_key) > 0) {
    s->stack_len = 0;
    s->current = NULL;
    return 0;
  }
  OMapEntry entry = {key, node_val(node, s->key_size)};
  memcpy(out, &entry, sizeof(OMapEntry));
  s->current = node->right;
  return 1;
}

static void omap_range_iter_drop(Iter *it) {
  OMapRangeIterState *s = it->state;
  if (it->allocator.free) {
    if (s->stack)
      it->allocator.free(it->allocator.ctx, s->stack);
    if (s->hi_key)
      it->allocator.free(it->allocator.ctx, s->hi_key);
    it->allocator.free(it->allocator.ctx, s);
  }
}

static void omap_push_lo(OMapRangeIterState *s, OMNode *node,
                         const void *lo_key) {
  while (node) {
    if (s->cmp(node_key(node), lo_key) < 0) {
      node = node->right;
    } else {
      if (s->stack_len == s->stack_cap) {
        size_t new_cap =
            s->stack_cap == 0 ? OMAP_ITER_STACK_INIT_CAP : s->stack_cap * 2;
        s->stack = s->allocator.realloc(
            s->allocator.ctx, s->stack, s->stack_cap * sizeof(OMNode *),
            new_cap * sizeof(OMNode *), _Alignof(OMNode *));
        s->stack_cap = new_cap;
      }
      s->stack[s->stack_len++] = node;
      node = node->left;
    }
  }
}

Iter omap_iter_range(const OMap *m, const void *lo_key, const void *hi_key) {
  if (!m)
    return (Iter){0};
  OMapRangeIterState *s = m->allocator.alloc(m->allocator.ctx, sizeof *s,
                                             _Alignof(OMapRangeIterState));
  void *hi_copy = NULL;
  if (hi_key) {
    hi_copy = m->allocator.alloc(m->allocator.ctx, m->key_size,
                                 _Alignof(max_align_t));
    memcpy(hi_copy, hi_key, m->key_size);
  }
  *s = (OMapRangeIterState){.stack = NULL,
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
  return (Iter){.next = omap_range_iter_next,
                .drop = omap_range_iter_drop,
                .state = s,
                .elem_size = sizeof(OMapEntry),
                .allocator = m->allocator};
}
