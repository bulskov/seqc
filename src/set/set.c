#include "set.h"

#include <stdbool.h>
#include <string.h>

#define SET_INITIAL_CAP 16

/* ---- internal helpers -------------------------------------------------- */

static size_t set_slot(const Set *s, const void *key) {
  return s->hash(key, s->elem_size) & (s->cap - 1);
}

/* Insert a bucket whose key is already allocated.  Does not touch s->len. */
static void set_insert_raw(Set *s, SetBucket incoming) {
  size_t slot = set_slot(s, incoming.key);
  incoming.psl = 1;
  for (;;) {
    SetBucket *cur = &s->buckets[slot];
    if (cur->psl == 0) {
      *cur = incoming;
      return;
    }
    /* Robin Hood: steal the slot from the "rich" (low psl) bucket */
    if (cur->psl < incoming.psl) {
      SetBucket tmp = *cur;
      *cur = incoming;
      incoming = tmp;
    }
    incoming.psl++;
    slot = (slot + 1) & (s->cap - 1);
  }
}

static void set_resize(Set *s) {
  size_t new_cap = s->cap * 2;
  SetBucket *nb = s->allocator.alloc(
      s->allocator.ctx, new_cap * sizeof(SetBucket), _Alignof(SetBucket));
  memset(nb, 0, new_cap * sizeof(SetBucket));

  SetBucket *old = s->buckets;
  size_t old_cap = s->cap;
  s->buckets = nb;
  s->cap = new_cap;

  for (size_t i = 0; i < old_cap; i++)
    if (old[i].psl > 0)
      set_insert_raw(s, (SetBucket){old[i].key, 0});

  if (s->allocator.free)
    s->allocator.free(s->allocator.ctx, old);
}

/* ---- public API -------------------------------------------------------- */

Set set_create(size_t elem_size, set_hash_fn hash, set_eq_fn eq,
               Allocator allocator) {
  return (Set){.buckets = NULL,
               .cap = 0,
               .len = 0,
               .elem_size = elem_size,
               .hash = hash,
               .eq = eq,
               .allocator = allocator};
}

bool set_contains(const Set *s, const void *elem) {
  if (!s || !elem || s->len == 0)
    return false;
  size_t slot = set_slot(s, elem);
  for (size_t i = 0; i < s->cap; i++) {
    size_t idx = (slot + i) & (s->cap - 1);
    const SetBucket *b = &s->buckets[idx];
    if (b->psl == 0)
      return false;
    if (s->eq(b->key, elem, s->elem_size))
      return true;
  }
  return false;
}

bool set_add(Set *s, const void *elem) {
  if (!s || !elem)
    return false;
  if (s->cap == 0) {
    s->buckets = s->allocator.alloc(s->allocator.ctx,
                                    SET_INITIAL_CAP * sizeof(SetBucket),
                                    _Alignof(SetBucket));
    memset(s->buckets, 0, SET_INITIAL_CAP * sizeof(SetBucket));
    s->cap = SET_INITIAL_CAP;
  }
  if (set_contains(s, elem))
    return false;
  if (s->len * 4 >= s->cap * 3)
    set_resize(s);
  void *key =
      s->allocator.alloc(s->allocator.ctx, s->elem_size, _Alignof(max_align_t));
  memcpy(key, elem, s->elem_size);
  set_insert_raw(s, (SetBucket){key, 0});
  s->len++;
  return true;
}

bool set_remove(Set *s, const void *elem) {
  if (!s || !elem || s->len == 0)
    return false;
  size_t slot = set_slot(s, elem);
  for (size_t i = 0; i < s->cap; i++) {
    size_t idx = (slot + i) & (s->cap - 1);
    SetBucket *b = &s->buckets[idx];
    if (b->psl == 0)
      return false;
    if (s->eq(b->key, elem, s->elem_size)) {
      if (s->allocator.free)
        s->allocator.free(s->allocator.ctx, b->key);
      /* backward shift to restore Robin Hood invariant */
      for (;;) {
        size_t next = (idx + 1) & (s->cap - 1);
        SetBucket *nb = &s->buckets[next];
        if (nb->psl <= 1) {
          s->buckets[idx] = (SetBucket){NULL, 0};
          break;
        }
        s->buckets[idx] = *nb;
        s->buckets[idx].psl--;
        idx = next;
      }
      s->len--;
      return true;
    }
  }
  return false;
}

size_t set_len(const Set *s) { return s ? s->len : 0; }

void set_free(Set *s) {
  if (!s || !s->buckets)
    return;
  if (s->allocator.free) {
    for (size_t i = 0; i < s->cap; i++)
      if (s->buckets[i].psl > 0)
        s->allocator.free(s->allocator.ctx, s->buckets[i].key);
    s->allocator.free(s->allocator.ctx, s->buckets);
  }
  s->buckets = NULL;
  s->cap = 0;
  s->len = 0;
}

void set_clear(Set *s) {
  if (!s || !s->buckets)
    return;
  if (s->allocator.free) {
    for (size_t i = 0; i < s->cap; i++)
      if (s->buckets[i].psl > 0)
        s->allocator.free(s->allocator.ctx, s->buckets[i].key);
  }
  memset(s->buckets, 0, s->cap * sizeof(SetBucket));
  s->len = 0;
}

/* ---- iter -------------------------------------------------------------- */

typedef struct {
  const SetBucket *buckets;
  size_t cap;
  size_t pos;
  size_t elem_size;
} SetIterState;

static bool set_iter_next(Iter *it, void *out) {
  SetIterState *s = it->state;
  while (s->pos < s->cap) {
    const SetBucket *b = &s->buckets[s->pos++];
    if (b->psl > 0) {
      memcpy(out, b->key, s->elem_size);
      return true;
    }
  }
  return false;
}

static void set_iter_drop(Iter *it) {
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, it->state);
}

Iter set_iter(const Set *s) {
  if (!s)
    return (Iter){0};
  SetIterState *state = s->allocator.alloc(s->allocator.ctx, sizeof *state,
                                           _Alignof(SetIterState));
  *state = (SetIterState){s->buckets, s->cap, 0, s->elem_size};
  return (Iter){.next = set_iter_next,
                .drop = set_iter_drop,
                .state = state,
                .elem_size = s->elem_size,
                .allocator = s->allocator};
}
