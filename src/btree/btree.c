#include "btree.h"

#include <stdbool.h>
#include <string.h>

/* ---- node layout ------------------------------------------------------- */

static void *node_data(const BTreeNode *node) {
  const size_t offset = (sizeof(BTreeNode) + _Alignof(max_align_t) - 1) &
                        ~(_Alignof(max_align_t) - 1);
  return (char *)node + offset;
}

static size_t node_alloc_size(size_t elem_size) {
  const size_t offset = (sizeof(BTreeNode) + _Alignof(max_align_t) - 1) &
                        ~(_Alignof(max_align_t) - 1);
  return offset + elem_size;
}

static BTreeNode *make_node(const BTree *t, const void *elem) {
  BTreeNode *node = t->allocator.alloc(
      t->allocator.ctx, node_alloc_size(t->elem_size), _Alignof(max_align_t));
  node->left = node->right = NULL;
  memcpy(node_data(node), elem, t->elem_size);
  return node;
}

/* ---- public API -------------------------------------------------------- */

BTree btree_create(size_t elem_size, compare_fn cmp, Allocator allocator) {
  return (BTree){.root = NULL,
                 .len = 0,
                 .elem_size = elem_size,
                 .cmp = cmp,
                 .allocator = allocator};
}

bool btree_insert(BTree *t, const void *elem) {
  if (!t || !elem)
    return false;
  BTreeNode **cur = &t->root;
  while (*cur) {
    int c = t->cmp(elem, node_data(*cur));
    if (c < 0)
      cur = &(*cur)->left;
    else if (c > 0)
      cur = &(*cur)->right;
    else
      return false; /* duplicate */
  }
  *cur = make_node(t, elem);
  t->len++;
  return true;
}

bool btree_contains(const BTree *t, const void *elem) {
  if (!t || !elem)
    return false;
  BTreeNode *cur = t->root;
  while (cur) {
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
static BTreeNode *do_remove(BTree *t, BTreeNode *node, const void *elem,
                            int *removed) {
  if (!node) {
    *removed = 0;
    return NULL;
  }
  int c = t->cmp(elem, node_data(node));
  if (c < 0) {
    node->left = do_remove(t, node->left, elem, removed);
  } else if (c > 0) {
    node->right = do_remove(t, node->right, elem, removed);
  } else {
    *removed = 1;
    if (!node->left) {
      BTreeNode *r = node->right;
      if (t->allocator.free)
        t->allocator.free(t->allocator.ctx, node);
      return r;
    }
    if (!node->right) {
      BTreeNode *l = node->left;
      if (t->allocator.free)
        t->allocator.free(t->allocator.ctx, node);
      return l;
    }
    /* two children: replace with in-order successor (min of right subtree),
     * copy its data here, then delete the successor below. */
    BTreeNode *succ = node->right;
    while (succ->left)
      succ = succ->left;
    memcpy(node_data(node), node_data(succ), t->elem_size);
    int dummy = 0;
    node->right = do_remove(t, node->right, node_data(node), &dummy);
  }
  return node;
}

bool btree_remove(BTree *t, const void *elem) {
  if (!t || !elem)
    return false;
  int removed = 0;
  t->root = do_remove(t, t->root, elem, &removed);
  if (removed)
    t->len--;
  return removed;
}

void *btree_min(const BTree *t) {
  if (!t || !t->root)
    return NULL;
  BTreeNode *cur = t->root;
  while (cur->left)
    cur = cur->left;
  return node_data(cur);
}

void *btree_max(const BTree *t) {
  if (!t || !t->root)
    return NULL;
  BTreeNode *cur = t->root;
  while (cur->right)
    cur = cur->right;
  return node_data(cur);
}

size_t btree_len(const BTree *t) { return t ? t->len : 0; }

static void free_subtree(BTree *t, BTreeNode *node) {
  if (!node)
    return;
  free_subtree(t, node->left);
  free_subtree(t, node->right);
  if (t->allocator.free)
    t->allocator.free(t->allocator.ctx, node);
}

void btree_free(BTree *t) {
  if (!t)
    return;
  free_subtree(t, t->root);
  t->root = NULL;
  t->len = 0;
}

void btree_clear(BTree *t) { btree_free(t); }

/* ---- iter: iterative in-order ----------------------------------------- */

#define BTREE_ITER_STACK_INIT_CAP 16

typedef struct {
  BTreeNode **stack;
  size_t stack_len;
  size_t stack_cap;
  BTreeNode *current; /* next node to push */
  size_t elem_size;
  Allocator allocator;
} BTreeIterState;

static bool btree_iter_next(Iter *it, void *out) {
  BTreeIterState *s = it->state;
  /* push left spine of current node, then pop and yield */
  while (s->current) {
    if (s->stack_len == s->stack_cap) {
      size_t new_cap =
          s->stack_cap == 0 ? BTREE_ITER_STACK_INIT_CAP : s->stack_cap * 2;
      s->stack = s->allocator.realloc(
          s->allocator.ctx, s->stack, s->stack_cap * sizeof(BTreeNode *),
          new_cap * sizeof(BTreeNode *), _Alignof(BTreeNode *));
      s->stack_cap = new_cap;
    }
    s->stack[s->stack_len++] = s->current;
    s->current = s->current->left;
  }
  if (s->stack_len == 0)
    return false;
  BTreeNode *node = s->stack[--s->stack_len];
  memcpy(out, node_data(node), s->elem_size);
  s->current = node->right;
  return true;
}

static void btree_iter_drop(Iter *it) {
  BTreeIterState *s = it->state;
  if (it->allocator.free) {
    if (s->stack)
      it->allocator.free(it->allocator.ctx, s->stack);
    it->allocator.free(it->allocator.ctx, s);
  }
}

Iter btree_iter(const BTree *t) {
  if (!t)
    return (Iter){0};
  BTreeIterState *s =
      t->allocator.alloc(t->allocator.ctx, sizeof *s, _Alignof(BTreeIterState));
  *s = (BTreeIterState){.stack = NULL,
                        .stack_len = 0,
                        .stack_cap = 0,
                        .current = t->root,
                        .elem_size = t->elem_size,
                        .allocator = t->allocator};
  return (Iter){.next = btree_iter_next,
                .drop = btree_iter_drop,
                .state = s,
                .elem_size = t->elem_size,
                .allocator = t->allocator};
}

static bool btree_iter_rev_next(Iter *it, void *out) {
  BTreeIterState *s = it->state;
  while (s->current) {
    if (s->stack_len == s->stack_cap) {
      size_t new_cap =
          s->stack_cap == 0 ? BTREE_ITER_STACK_INIT_CAP : s->stack_cap * 2;
      s->stack = s->allocator.realloc(
          s->allocator.ctx, s->stack, s->stack_cap * sizeof(BTreeNode *),
          new_cap * sizeof(BTreeNode *), _Alignof(BTreeNode *));
      s->stack_cap = new_cap;
    }
    s->stack[s->stack_len++] = s->current;
    s->current = s->current->right;
  }
  if (s->stack_len == 0)
    return false;
  BTreeNode *node = s->stack[--s->stack_len];
  memcpy(out, node_data(node), s->elem_size);
  s->current = node->left;
  return true;
}

Iter btree_iter_rev(const BTree *t) {
  if (!t)
    return (Iter){0};
  BTreeIterState *s =
      t->allocator.alloc(t->allocator.ctx, sizeof *s, _Alignof(BTreeIterState));
  *s = (BTreeIterState){.stack = NULL,
                        .stack_len = 0,
                        .stack_cap = 0,
                        .current = t->root,
                        .elem_size = t->elem_size,
                        .allocator = t->allocator};
  return (Iter){.next = btree_iter_rev_next,
                .drop = btree_iter_drop,
                .state = s,
                .elem_size = t->elem_size,
                .allocator = t->allocator};
}

/* ---- range iterator ---------------------------------------------------- */

typedef struct {
  BTreeNode **stack;
  size_t stack_len;
  size_t stack_cap;
  BTreeNode *current;
  size_t elem_size;
  Allocator allocator;
  compare_fn cmp;
  void *hi; /* NULL means no upper bound; owned allocation */
} BTreeRangeIterState;

static bool btree_range_iter_next(Iter *it, void *out) {
  BTreeRangeIterState *s = it->state;
  while (s->current) {
    if (s->stack_len == s->stack_cap) {
      size_t new_cap =
          s->stack_cap == 0 ? BTREE_ITER_STACK_INIT_CAP : s->stack_cap * 2;
      s->stack = s->allocator.realloc(
          s->allocator.ctx, s->stack, s->stack_cap * sizeof(BTreeNode *),
          new_cap * sizeof(BTreeNode *), _Alignof(BTreeNode *));
      s->stack_cap = new_cap;
    }
    s->stack[s->stack_len++] = s->current;
    s->current = s->current->left;
  }
  if (s->stack_len == 0)
    return false;
  BTreeNode *node = s->stack[--s->stack_len];
  void *data = node_data(node);
  if (s->hi && s->cmp(data, s->hi) > 0) {
    s->stack_len = 0;
    s->current = NULL;
    return false;
  }
  memcpy(out, data, s->elem_size);
  s->current = node->right;
  return true;
}

static void btree_range_iter_drop(Iter *it) {
  BTreeRangeIterState *s = it->state;
  if (it->allocator.free) {
    if (s->stack)
      it->allocator.free(it->allocator.ctx, s->stack);
    if (s->hi)
      it->allocator.free(it->allocator.ctx, s->hi);
    it->allocator.free(it->allocator.ctx, s);
  }
}

static void btree_push_lo(BTreeRangeIterState *s, BTreeNode *node,
                          const void *lo) {
  while (node) {
    if (s->cmp(node_data(node), lo) < 0) {
      node = node->right;
    } else {
      if (s->stack_len == s->stack_cap) {
        size_t new_cap =
            s->stack_cap == 0 ? BTREE_ITER_STACK_INIT_CAP : s->stack_cap * 2;
        s->stack = s->allocator.realloc(
            s->allocator.ctx, s->stack, s->stack_cap * sizeof(BTreeNode *),
            new_cap * sizeof(BTreeNode *), _Alignof(BTreeNode *));
        s->stack_cap = new_cap;
      }
      s->stack[s->stack_len++] = node;
      node = node->left;
    }
  }
}

Iter btree_iter_range(const BTree *t, const void *lo, const void *hi) {
  if (!t)
    return (Iter){0};
  BTreeRangeIterState *s = t->allocator.alloc(t->allocator.ctx, sizeof *s,
                                              _Alignof(BTreeRangeIterState));
  void *hi_copy = NULL;
  if (hi) {
    hi_copy = t->allocator.alloc(t->allocator.ctx, t->elem_size,
                                 _Alignof(max_align_t));
    memcpy(hi_copy, hi, t->elem_size);
  }
  *s = (BTreeRangeIterState){.stack = NULL,
                             .stack_len = 0,
                             .stack_cap = 0,
                             .current = NULL,
                             .elem_size = t->elem_size,
                             .allocator = t->allocator,
                             .cmp = t->cmp,
                             .hi = hi_copy};
  if (lo)
    btree_push_lo(s, t->root, lo);
  else
    s->current = t->root;
  return (Iter){.next = btree_range_iter_next,
                .drop = btree_range_iter_drop,
                .state = s,
                .elem_size = t->elem_size,
                .allocator = t->allocator};
}
