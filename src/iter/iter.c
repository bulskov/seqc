#include "iter.h"
#include "arena/arena.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ---- iter_from_slice -------------------------------------------------- */

typedef struct {
  const char *ptr;
  size_t len;
  size_t elem_size;
  size_t pos;
} SliceIterState;

static int slice_next(Iter *it, void *out) {
  if (!it || !it->state || !out) {
    return 0; /* invalid iterator or output buffer */
  }

  SliceIterState *s = it->state;
  if (s->pos >= s->len)
    return 0;
  memcpy(out, s->ptr + s->pos * s->elem_size, s->elem_size);
  s->pos++;
  return 1;
}

static void slice_drop(Iter *it) {
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, it->state);
}

Iter iter_from_slice(Slice s, Allocator allocator) {
  SliceIterState *state =
      allocator.alloc(allocator.ctx, sizeof *state, _Alignof(SliceIterState));
  *state = (SliceIterState){s.ptr, s.len, s.elem_size, 0};
  return (Iter){.next = slice_next,
                .drop = slice_drop,
                .state = state,
                .elem_size = s.elem_size,
                .allocator = allocator};
}

static int slice_rev_next(Iter *it, void *out) {
  SliceIterState *s = it->state;
  if (!s->pos)
    return 0;
  s->pos--;
  memcpy(out, s->ptr + s->pos * s->elem_size, s->elem_size);
  return 1;
}

Iter iter_from_slice_rev(Slice s, Allocator allocator) {
  SliceIterState *state =
      allocator.alloc(allocator.ctx, sizeof *state, _Alignof(SliceIterState));
  *state = (SliceIterState){s.ptr, s.len, s.elem_size, s.len};
  return (Iter){.next = slice_rev_next,
                .drop = slice_drop,
                .state = state,
                .elem_size = s.elem_size,
                .allocator = allocator};
}

/* ---- iter_filter ------------------------------------------------------- */

typedef struct {
  Iter source;
  pred_fn pred;
  void *ctx;
} FilterState;

static int filter_next(Iter *it, void *out) {
  if (!it || !it->state || !out) {
    return 0; /* invalid iterator or output buffer */
  }
  FilterState *s = it->state;
  while (s->source.next(&s->source, out))
    if (s->pred(out, s->ctx))
      return 1;
  return 0;
}

static void filter_drop(Iter *it) {
  if (!it->state)
    return;
  FilterState *s = it->state;
  iter_drop(&s->source);
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, s);
}

Iter iter_filter(Iter source, pred_fn pred, void *ctx) {
  FilterState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                          _Alignof(FilterState));
  *s = (FilterState){source, pred, ctx};
  return (Iter){.next = filter_next,
                .drop = filter_drop,
                .state = s,
                .elem_size = source.elem_size,
                .allocator = source.allocator};
}

/* ---- iter_map ---------------------------------------------------------- */

typedef struct {
  Iter source;
  map_fn map;
  void *ctx;
  void *in_buf; /* reusable buffer of source.elem_size bytes */
} MapState;

static int map_next(Iter *it, void *out) {
  if (!it || !it->state || !out) {
    return 0; /* invalid iterator or output buffer */
  }
  MapState *s = it->state;
  if (!s->source.next(&s->source, s->in_buf))
    return 0;
  s->map(s->in_buf, out, s->ctx);
  return 1;
}

static void map_drop(Iter *it) {
  if (!it->state)
    return;
  MapState *s = it->state;
  iter_drop(&s->source);
  if (it->allocator.free) {
    it->allocator.free(it->allocator.ctx, s->in_buf);
    it->allocator.free(it->allocator.ctx, s);
  }
}

Iter iter_map(Iter source, map_fn map, void *ctx, size_t out_elem_size) {
  MapState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                       _Alignof(MapState));
  void *in_buf = source.allocator.alloc(source.allocator.ctx, source.elem_size,
                                        _Alignof(max_align_t));
  *s = (MapState){source, map, ctx, in_buf};
  return (Iter){.next = map_next,
                .drop = map_drop,
                .state = s,
                .elem_size = out_elem_size,
                .allocator = source.allocator};
}

/* ---- iter_take --------------------------------------------------------- */

typedef struct {
  Iter source;
  size_t remaining;
} TakeState;

static int take_next(Iter *it, void *out) {
  TakeState *s = it->state;
  if (s->remaining == 0)
    return 0;
  if (!s->source.next(&s->source, out))
    return 0;
  s->remaining--;
  return 1;
}

static void take_drop(Iter *it) {
  if (!it->state)
    return;
  TakeState *s = it->state;
  iter_drop(&s->source);
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, s);
}

Iter iter_take(Iter source, size_t n) {
  TakeState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                        _Alignof(TakeState));
  *s = (TakeState){source, n};
  return (Iter){.next = take_next,
                .drop = take_drop,
                .state = s,
                .elem_size = source.elem_size,
                .allocator = source.allocator};
}

/* ---- iter_skip --------------------------------------------------------- */

typedef struct {
  Iter source;
  size_t remaining;
} SkipState;

static int skip_next(Iter *it, void *out) {
  SkipState *s = it->state;
  /* drain skipped elements once, then pass through */
  while (s->remaining > 0) {
    if (!s->source.next(&s->source, out))
      return 0;
    s->remaining--;
  }
  return s->source.next(&s->source, out);
}

static void skip_drop(Iter *it) {
  if (!it->state)
    return;
  SkipState *s = it->state;
  iter_drop(&s->source);
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, s);
}

Iter iter_skip(Iter source, size_t n) {
  SkipState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                        _Alignof(SkipState));
  *s = (SkipState){source, n};
  return (Iter){.next = skip_next,
                .drop = skip_drop,
                .state = s,
                .elem_size = source.elem_size,
                .allocator = source.allocator};
}

/* ---- terminals --------------------------------------------------------- */

Slice iter_collect(Iter it) {
  const size_t elem_size = it.elem_size;
  size_t cap = 16;
  size_t len = 0;
  char *buf = it.allocator.alloc(it.allocator.ctx, cap * elem_size,
                                 _Alignof(max_align_t));
  char *tmp =
      it.allocator.alloc(it.allocator.ctx, elem_size, _Alignof(max_align_t));

  while (it.next(&it, tmp)) {
    if (len == cap) {
      cap *= 2;
      buf = it.allocator.realloc(it.allocator.ctx, buf, len * elem_size,
                                 cap * elem_size, _Alignof(max_align_t));
    }
    memcpy(buf + len * elem_size, tmp, elem_size);
    len++;
  }

  if (it.allocator.free) {
    it.allocator.free(it.allocator.ctx, tmp);
  }
  iter_drop(&it);

  if (len == 0) {
    if (it.allocator.free) {
      it.allocator.free(it.allocator.ctx, buf);
    }
    return (Slice){NULL, 0, elem_size};
  }

  void *out = it.allocator.alloc(it.allocator.ctx, len * elem_size,
                                 _Alignof(max_align_t));
  memcpy(out, buf, len * elem_size);
  if (it.allocator.free) {
    it.allocator.free(it.allocator.ctx, buf);
  }
  return (Slice){out, len, elem_size};
}

size_t iter_count(Iter it) {
  size_t n = 0;
  void *tmp =
      it.allocator.alloc(it.allocator.ctx, it.elem_size, _Alignof(max_align_t));
  while (it.next(&it, tmp))
    n++;
  if (it.allocator.free)
    it.allocator.free(it.allocator.ctx, tmp);
  iter_drop(&it);
  return n;
}

void iter_foreach(Iter it, visitor_fn visit, void *ctx) {
  void *tmp =
      it.allocator.alloc(it.allocator.ctx, it.elem_size, _Alignof(max_align_t));
  while (it.next(&it, tmp))
    visit(tmp, ctx);
  if (it.allocator.free) {
    it.allocator.free(it.allocator.ctx, tmp);
  }
  iter_drop(&it);
}

void iter_reduce(Iter it, void *acc, combine_fn combine, void *ctx) {
  void *tmp =
      it.allocator.alloc(it.allocator.ctx, it.elem_size, _Alignof(max_align_t));
  while (it.next(&it, tmp))
    combine(acc, tmp, ctx);
  if (it.allocator.free) {
    it.allocator.free(it.allocator.ctx, tmp);
  }
  iter_drop(&it);
}

/* ---- iter_chain -------------------------------------------------------- */

typedef struct {
  Iter first;
  Iter second;
  int done_first;
} ChainState;

static int chain_next(Iter *it, void *out) {
  ChainState *s = it->state;
  if (!s->done_first) {
    if (s->first.next(&s->first, out))
      return 1;
    s->done_first = 1;
  }
  return s->second.next(&s->second, out);
}

static void chain_drop(Iter *it) {
  if (!it->state)
    return;
  ChainState *s = it->state;
  iter_drop(&s->first);
  iter_drop(&s->second);
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, s);
}

Iter iter_chain(Iter a, Iter b) {
  ChainState *s =
      a.allocator.alloc(a.allocator.ctx, sizeof *s, _Alignof(ChainState));
  *s = (ChainState){a, b, 0};
  return (Iter){.next = chain_next,
                .drop = chain_drop,
                .state = s,
                .elem_size = a.elem_size,
                .allocator = a.allocator};
}

/* ---- iter_zip ---------------------------------------------------------- */

typedef struct {
  Iter a;
  Iter b;
  size_t a_elem_size;
  void *buf_a; /* temporary buffer; confirms a before writing b to out */
} ZipState;

static int zip_next(Iter *it, void *out) {
  ZipState *s = it->state;
  if (!s->a.next(&s->a, s->buf_a))
    return 0;
  if (!s->b.next(&s->b, (char *)out + s->a_elem_size))
    return 0;
  memcpy(out, s->buf_a, s->a_elem_size);
  return 1;
}

static void zip_drop(Iter *it) {
  if (!it->state)
    return;
  ZipState *s = it->state;
  iter_drop(&s->a);
  iter_drop(&s->b);
  if (it->allocator.free) {
    it->allocator.free(it->allocator.ctx, s->buf_a);
    it->allocator.free(it->allocator.ctx, s);
  }
}

Iter iter_zip(Iter a, Iter b) {
  ZipState *s =
      a.allocator.alloc(a.allocator.ctx, sizeof *s, _Alignof(ZipState));
  void *buf_a =
      a.allocator.alloc(a.allocator.ctx, a.elem_size, _Alignof(max_align_t));
  *s = (ZipState){a, b, a.elem_size, buf_a};
  return (Iter){.next = zip_next,
                .drop = zip_drop,
                .state = s,
                .elem_size = a.elem_size + b.elem_size,
                .allocator = a.allocator};
}

/* ---- iter_sort --------------------------------------------------------- */

Slice iter_sort(Iter it, compare_fn cmp) {
  Slice s = iter_collect(it);
  if (s.len > 1)
    qsort(s.ptr, s.len, s.elem_size, cmp);
  return s;
}

/* ---- iter_find --------------------------------------------------------- */

int iter_find(Iter it, pred_fn pred, void *ctx, void *out) {
  void *tmp =
      it.allocator.alloc(it.allocator.ctx, it.elem_size, _Alignof(max_align_t));
  int found = 0;
  while (it.next(&it, tmp)) {
    if (pred(tmp, ctx)) {
      if (out)
        memcpy(out, tmp, it.elem_size);
      found = 1;
      break;
    }
  }
  if (it.allocator.free)
    it.allocator.free(it.allocator.ctx, tmp);
  iter_drop(&it);
  return found;
}

/* ---- iter_any ---------------------------------------------------------- */

int iter_any(Iter it, pred_fn pred, void *ctx) {
  void *tmp =
      it.allocator.alloc(it.allocator.ctx, it.elem_size, _Alignof(max_align_t));
  int found = 0;
  while (!found && it.next(&it, tmp))
    if (pred(tmp, ctx))
      found = 1;
  if (it.allocator.free)
    it.allocator.free(it.allocator.ctx, tmp);
  iter_drop(&it);
  return found;
}

/* ---- iter_all ---------------------------------------------------------- */

int iter_all(Iter it, pred_fn pred, void *ctx) {
  void *tmp =
      it.allocator.alloc(it.allocator.ctx, it.elem_size, _Alignof(max_align_t));
  int all = 1;
  while (all && it.next(&it, tmp))
    if (!pred(tmp, ctx))
      all = 0;
  if (it.allocator.free)
    it.allocator.free(it.allocator.ctx, tmp);
  iter_drop(&it);
  return all;
}
