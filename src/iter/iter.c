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

static bool slice_next(Iter *it, void *out) {
  if (!it || !it->state || !out) {
    return false; /* invalid iterator or output buffer */
  }

  SliceIterState *s = it->state;
  if (s->pos >= s->len)
    return false;
  memcpy(out, s->ptr + s->pos * s->elem_size, s->elem_size);
  s->pos++;
  return true;
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

static bool slice_rev_next(Iter *it, void *out) {
  SliceIterState *s = it->state;
  if (!s->pos)
    return false;
  s->pos--;
  memcpy(out, s->ptr + s->pos * s->elem_size, s->elem_size);
  return true;
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

/* ---- iter_generate ----------------------------------------------------- */

typedef struct {
  generate_fn fn;
  void *ctx;
} GenerateState;

static bool generate_next(Iter *it, void *out) {
  GenerateState *s = it->state;
  return s->fn(out, s->ctx);
}

static void generate_drop(Iter *it) {
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, it->state);
}

Iter iter_generate(generate_fn fn, void *ctx, size_t elem_size,
                   Allocator allocator) {
  GenerateState *s =
      allocator.alloc(allocator.ctx, sizeof *s, _Alignof(GenerateState));
  *s = (GenerateState){fn, ctx};
  return (Iter){.next = generate_next,
                .drop = generate_drop,
                .state = s,
                .elem_size = elem_size,
                .allocator = allocator};
}

/* ---- iter_range -------------------------------------------------------- */

#include <limits.h>

typedef struct {
  long long cur;
  long long end;
  long long step;
} RangeState;

static bool range_next(Iter *it, void *out) {
  RangeState *s = it->state;
  if (s->step > 0 && s->cur >= s->end)
    return false;
  if (s->step < 0 && s->cur <= s->end)
    return false;
  memcpy(out, &s->cur, sizeof(long long));
  s->cur += s->step;
  return true;
}

static void range_drop(Iter *it) {
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, it->state);
}

Iter iter_range(long long start, long long end, long long step,
                Allocator allocator) {
  RangeState *s =
      allocator.alloc(allocator.ctx, sizeof *s, _Alignof(RangeState));
  *s = (RangeState){start, end, step};
  return (Iter){.next = range_next,
                .drop = range_drop,
                .state = s,
                .elem_size = sizeof(long long),
                .allocator = allocator};
}

/* ---- iter_filter ------------------------------------------------------- */

typedef struct {
  Iter source;
  pred_fn pred;
  void *ctx;
} FilterState;

static bool filter_next(Iter *it, void *out) {
  if (!it || !it->state || !out) {
    return false; /* invalid iterator or output buffer */
  }
  FilterState *s = it->state;
  while (s->source.next(&s->source, out))
    if (s->pred(out, s->ctx))
      return true;
  return false;
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

static bool map_next(Iter *it, void *out) {
  if (!it || !it->state || !out) {
    return false; /* invalid iterator or output buffer */
  }
  MapState *s = it->state;
  if (!s->source.next(&s->source, s->in_buf))
    return false;
  s->map(s->in_buf, out, s->ctx);
  return true;
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

static bool take_next(Iter *it, void *out) {
  TakeState *s = it->state;
  if (s->remaining == 0)
    return false;
  if (!s->source.next(&s->source, out))
    return false;
  s->remaining--;
  return true;
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

/* ---- iter_take_while ---------------------------------------------------- */

typedef struct {
  Iter source;
  pred_fn pred;
  void *ctx;
} TakeWhileState;

static bool take_while_next(Iter *it, void *out) {
  TakeWhileState *s = it->state;
  if (!s->source.next(&s->source, out))
    return false;
  if (!s->pred(out, s->ctx))
    return false;
  return true;
}

static void take_while_drop(Iter *it) {
  if (!it->state)
    return;
  TakeWhileState *s = it->state;
  iter_drop(&s->source);
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, s);
}

Iter iter_take_while(Iter source, pred_fn pred, void *ctx) {
  TakeWhileState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                             _Alignof(TakeWhileState));
  *s = (TakeWhileState){source, pred, ctx};
  return (Iter){.next = take_while_next,
                .drop = take_while_drop,
                .state = s,
                .elem_size = source.elem_size,
                .allocator = source.allocator};
}

/* ---- iter_skip --------------------------------------------------------- */

typedef struct {
  Iter source;
  size_t remaining;
} SkipState;

static bool skip_next(Iter *it, void *out) {
  SkipState *s = it->state;
  /* drain skipped elements once, then pass through */
  while (s->remaining > 0) {
    if (!s->source.next(&s->source, out))
      return false;
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

/* ---- iter_skip_while ---------------------------------------------------- */

typedef struct {
  Iter source;
  pred_fn pred;
  void *ctx;
  bool done_skipping;
} SkipWhileState;

static bool skip_while_next(Iter *it, void *out) {
  SkipWhileState *s = it->state;
  while (s->source.next(&s->source, out)) {
    if (s->done_skipping || !s->pred(out, s->ctx)) {
      s->done_skipping = true;
      return true;
    }
  }
  return false;
}

static void skip_while_drop(Iter *it) {
  if (!it->state)
    return;
  SkipWhileState *s = it->state;
  iter_drop(&s->source);
  if (it->allocator.free)
    it->allocator.free(it->allocator.ctx, s);
}

Iter iter_skip_while(Iter source, pred_fn pred, void *ctx) {
  SkipWhileState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                             _Alignof(SkipWhileState));
  *s = (SkipWhileState){source, pred, ctx, false};
  return (Iter){.next = skip_while_next,
                .drop = skip_while_drop,
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

static bool chain_next(Iter *it, void *out) {
  ChainState *s = it->state;
  if (!s->done_first) {
    if (s->first.next(&s->first, out))
      return true;
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

static bool zip_next(Iter *it, void *out) {
  ZipState *s = it->state;
  if (!s->a.next(&s->a, s->buf_a))
    return false;
  if (!s->b.next(&s->b, (char *)out + s->a_elem_size))
    return false;
  memcpy(out, s->buf_a, s->a_elem_size);
  return true;
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

bool iter_find(Iter it, pred_fn pred, void *ctx, void *out) {
  void *tmp =
      it.allocator.alloc(it.allocator.ctx, it.elem_size, _Alignof(max_align_t));
  bool found = false;
  while (it.next(&it, tmp)) {
    if (pred(tmp, ctx)) {
      if (out)
        memcpy(out, tmp, it.elem_size);
      found = true;
      break;
    }
  }
  if (it.allocator.free)
    it.allocator.free(it.allocator.ctx, tmp);
  iter_drop(&it);
  return found;
}

/* ---- iter_any ---------------------------------------------------------- */

bool iter_any(Iter it, pred_fn pred, void *ctx) {
  void *tmp =
      it.allocator.alloc(it.allocator.ctx, it.elem_size, _Alignof(max_align_t));
  bool found = false;
  while (!found && it.next(&it, tmp))
    if (pred(tmp, ctx))
      found = true;
  if (it.allocator.free)
    it.allocator.free(it.allocator.ctx, tmp);
  iter_drop(&it);
  return found;
}

/* ---- iter_all ---------------------------------------------------------- */

bool iter_all(Iter it, pred_fn pred, void *ctx) {
  void *tmp =
      it.allocator.alloc(it.allocator.ctx, it.elem_size, _Alignof(max_align_t));
  bool all = true;
  while (all && it.next(&it, tmp))
    if (!pred(tmp, ctx))
      all = false;
  if (it.allocator.free)
    it.allocator.free(it.allocator.ctx, tmp);
  iter_drop(&it);
  return all;
}

/* ---- iter_enumerate ---------------------------------------------------- */

typedef struct {
  Iter source;
  size_t index;
  void *buf; /* holds the current element so EnumEntry.elem is valid */
} EnumState;

static bool enum_next(Iter *it, void *out) {
  EnumState *s = it->state;
  if (!s->source.next(&s->source, s->buf))
    return false;
  EnumEntry entry = {s->index++, s->buf};
  memcpy(out, &entry, sizeof(EnumEntry));
  return true;
}

static void enum_drop(Iter *it) {
  EnumState *s = it->state;
  iter_drop(&s->source);
  if (it->allocator.free) {
    it->allocator.free(it->allocator.ctx, s->buf);
    it->allocator.free(it->allocator.ctx, s);
  }
}

Iter iter_enumerate(Iter source) {
  EnumState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                        _Alignof(EnumState));
  void *buf = source.allocator.alloc(source.allocator.ctx, source.elem_size,
                                     _Alignof(max_align_t));
  *s = (EnumState){source, 0, buf};
  return (Iter){.next = enum_next,
                .drop = enum_drop,
                .state = s,
                .elem_size = sizeof(EnumEntry),
                .allocator = source.allocator};
}

/* ---- iter_window ------------------------------------------------------- */

typedef struct {
  Iter source;
  char *buf; /* n * elem_size bytes                  */
  size_t n;
  size_t elem_size;
  int primed; /* 1 once the first window is filled    */
  Allocator allocator;
} WindowState;

static bool window_next(Iter *it, void *out) {
  WindowState *s = it->state;
  if (!s->primed) {
    /* Fill first n elements */
    for (size_t i = 0; i < s->n; i++) {
      if (!s->source.next(&s->source, s->buf + i * s->elem_size))
        return false; /* source shorter than n */
    }
    s->primed = 1;
  } else {
    /* Shift left by one, read new element at end */
    memmove(s->buf, s->buf + s->elem_size, (s->n - 1) * s->elem_size);
    if (!s->source.next(&s->source, s->buf + (s->n - 1) * s->elem_size))
      return false;
  }
  Slice sl = {s->buf, s->n, s->elem_size};
  memcpy(out, &sl, sizeof(Slice));
  return true;
}

static void window_drop(Iter *it) {
  WindowState *s = it->state;
  iter_drop(&s->source);
  if (it->allocator.free) {
    it->allocator.free(it->allocator.ctx, s->buf);
    it->allocator.free(it->allocator.ctx, s);
  }
}

Iter iter_window(Iter source, size_t n) {
  WindowState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                          _Alignof(WindowState));
  char *buf = source.allocator.alloc(source.allocator.ctx, n * source.elem_size,
                                     _Alignof(max_align_t));
  *s = (WindowState){source, buf, n, source.elem_size, 0, source.allocator};
  return (Iter){.next = window_next,
                .drop = window_drop,
                .state = s,
                .elem_size = sizeof(Slice),
                .allocator = source.allocator};
}

/* ---- iter_chunks ------------------------------------------------------- */

typedef struct {
  Iter source;
  char *buf; /* n * elem_size bytes */
  size_t n;
  size_t elem_size;
  int done;
  Allocator allocator;
} ChunkState;

static bool chunk_next(Iter *it, void *out) {
  ChunkState *s = it->state;
  if (s->done)
    return false;
  size_t count = 0;
  while (count < s->n &&
         s->source.next(&s->source, s->buf + count * s->elem_size))
    count++;
  if (count == 0)
    return false;
  if (count < s->n)
    s->done = 1; /* last partial chunk */
  Slice sl = {s->buf, count, s->elem_size};
  memcpy(out, &sl, sizeof(Slice));
  return true;
}

static void chunk_drop(Iter *it) {
  ChunkState *s = it->state;
  iter_drop(&s->source);
  if (it->allocator.free) {
    it->allocator.free(it->allocator.ctx, s->buf);
    it->allocator.free(it->allocator.ctx, s);
  }
}

Iter iter_chunks(Iter source, size_t n) {
  ChunkState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                         _Alignof(ChunkState));
  char *buf = source.allocator.alloc(source.allocator.ctx, n * source.elem_size,
                                     _Alignof(max_align_t));
  *s = (ChunkState){source, buf, n, source.elem_size, 0, source.allocator};
  return (Iter){.next = chunk_next,
                .drop = chunk_drop,
                .state = s,
                .elem_size = sizeof(Slice),
                .allocator = source.allocator};
}

/* ---- iter_flat_map ----------------------------------------------------- */

typedef struct {
  Iter source; /* outer iterator                        */
  flat_map_fn fn;
  void *ctx;
  void *elem_buf; /* one element from source               */
  Iter sub;       /* current sub-iterator (zeroed = none)  */
  int sub_active; /* 1 when sub holds a live iterator      */
  size_t out_elem_size;
  Allocator allocator;
} FlatMapState;

static bool flat_map_next(Iter *it, void *out) {
  FlatMapState *s = it->state;
  while (1) {
    /* Drain current sub-iterator */
    if (s->sub_active) {
      if (s->sub.next(&s->sub, out))
        return true;
      iter_drop(&s->sub);
      s->sub_active = 0;
    }
    /* Pull next element from source */
    if (!s->source.next(&s->source, s->elem_buf))
      return false;
    /* Produce next sub-iterator */
    s->fn(s->elem_buf, &s->sub, s->ctx);
    s->sub_active = 1;
  }
}

static void flat_map_drop(Iter *it) {
  FlatMapState *s = it->state;
  if (s->sub_active)
    iter_drop(&s->sub);
  iter_drop(&s->source);
  if (it->allocator.free) {
    it->allocator.free(it->allocator.ctx, s->elem_buf);
    it->allocator.free(it->allocator.ctx, s);
  }
}

Iter iter_flat_map(Iter source, flat_map_fn fn, void *ctx,
                   size_t out_elem_size) {
  FlatMapState *s = source.allocator.alloc(source.allocator.ctx, sizeof *s,
                                           _Alignof(FlatMapState));
  void *elem_buf = source.allocator.alloc(
      source.allocator.ctx, source.elem_size, _Alignof(max_align_t));
  *s = (FlatMapState){.source = source,
                      .fn = fn,
                      .ctx = ctx,
                      .elem_buf = elem_buf,
                      .sub = {0},
                      .sub_active = 0,
                      .out_elem_size = out_elem_size,
                      .allocator = source.allocator};
  return (Iter){.next = flat_map_next,
                .drop = flat_map_drop,
                .state = s,
                .elem_size = out_elem_size,
                .allocator = source.allocator};
}

/* ---- iter_min / iter_max ----------------------------------------------- */

bool iter_min(Iter it, compare_fn cmp, void *out) {
  size_t elem_size = it.elem_size;
  char *best =
      it.allocator.alloc(it.allocator.ctx, elem_size, _Alignof(max_align_t));
  char *cur =
      it.allocator.alloc(it.allocator.ctx, elem_size, _Alignof(max_align_t));
  bool found = false;
  while (it.next(&it, cur)) {
    if (!found || cmp(cur, best) < 0)
      memcpy(best, cur, elem_size);
    found = true;
  }
  if (found && out)
    memcpy(out, best, elem_size);
  if (it.allocator.free) {
    it.allocator.free(it.allocator.ctx, best);
    it.allocator.free(it.allocator.ctx, cur);
  }
  iter_drop(&it);
  return found;
}

bool iter_max(Iter it, compare_fn cmp, void *out) {
  size_t elem_size = it.elem_size;
  char *best =
      it.allocator.alloc(it.allocator.ctx, elem_size, _Alignof(max_align_t));
  char *cur =
      it.allocator.alloc(it.allocator.ctx, elem_size, _Alignof(max_align_t));
  bool found = false;
  while (it.next(&it, cur)) {
    if (!found || cmp(cur, best) > 0)
      memcpy(best, cur, elem_size);
    found = true;
  }
  if (found && out)
    memcpy(out, best, elem_size);
  if (it.allocator.free) {
    it.allocator.free(it.allocator.ctx, best);
    it.allocator.free(it.allocator.ctx, cur);
  }
  iter_drop(&it);
  return found;
}
