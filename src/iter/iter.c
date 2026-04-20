#include "iter.h"
#include "arena/arena.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ---- shared helper ---------------------------------------------------- */

static void free_state(Iter *it) { free(it->state); }

/* ---- iter_from_slice -------------------------------------------------- */

typedef struct {
  const char *ptr;
  size_t len;
  size_t elem_size;
  size_t pos;
} SliceIterState;

static int slice_next(Iter *it, void *out) {
  SliceIterState *s = it->state;
  if (s->pos >= s->len)
    return 0;
  memcpy(out, s->ptr + s->pos * s->elem_size, s->elem_size);
  s->pos++;
  return 1;
}

Iter iter_from_slice(Slice s) {
  SliceIterState *state = malloc(sizeof *state);
  *state = (SliceIterState){s.ptr, s.len, s.elem_size, 0};
  return (Iter){.next = slice_next,
                .drop = free_state,
                .state = state,
                .elem_size = s.elem_size};
}

/* ---- iter_filter ------------------------------------------------------- */

typedef struct {
  Iter source;
  int (*pred)(const void *elem, void *ctx);
  void *ctx;
} FilterState;

static int filter_next(Iter *it, void *out) {
  FilterState *s = it->state;
  while (s->source.next(&s->source, out))
    if (s->pred(out, s->ctx))
      return 1;
  return 0;
}

static void filter_drop(Iter *it) {
  FilterState *s = it->state;
  iter_drop(&s->source);
  free(s);
}

Iter iter_filter(Iter source, int (*pred)(const void *elem, void *ctx),
                 void *ctx) {
  FilterState *s = malloc(sizeof *s);
  *s = (FilterState){source, pred, ctx};
  return (Iter){.next = filter_next,
                .drop = filter_drop,
                .state = s,
                .elem_size = source.elem_size};
}

/* ---- iter_map ---------------------------------------------------------- */

typedef struct {
  Iter source;
  void (*map)(const void *in, void *out, void *ctx);
  void *ctx;
  void *in_buf; /* reusable buffer of source.elem_size bytes */
} MapState;

static int map_next(Iter *it, void *out) {
  MapState *s = it->state;
  if (!s->source.next(&s->source, s->in_buf))
    return 0;
  s->map(s->in_buf, out, s->ctx);
  return 1;
}

static void map_drop(Iter *it) {
  MapState *s = it->state;
  iter_drop(&s->source);
  free(s->in_buf);
  free(s);
}

Iter iter_map(Iter source, void (*map)(const void *in, void *out, void *ctx),
              void *ctx, size_t out_elem_size) {
  MapState *s = malloc(sizeof *s);
  s->source = source;
  s->map = map;
  s->ctx = ctx;
  s->in_buf = malloc(source.elem_size);
  return (Iter){.next = map_next,
                .drop = map_drop,
                .state = s,
                .elem_size = out_elem_size};
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
  TakeState *s = it->state;
  iter_drop(&s->source);
  free(s);
}

Iter iter_take(Iter source, size_t n) {
  TakeState *s = malloc(sizeof *s);
  *s = (TakeState){source, n};
  return (Iter){.next = take_next,
                .drop = take_drop,
                .state = s,
                .elem_size = source.elem_size};
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
  SkipState *s = it->state;
  iter_drop(&s->source);
  free(s);
}

Iter iter_skip(Iter source, size_t n) {
  SkipState *s = malloc(sizeof *s);
  *s = (SkipState){source, n};
  return (Iter){.next = skip_next,
                .drop = skip_drop,
                .state = s,
                .elem_size = source.elem_size};
}

/* ---- terminals --------------------------------------------------------- */

Slice iter_collect(Iter it, Arena *a) {
  const size_t elem_size = it.elem_size;
  size_t cap = 16;
  size_t len = 0;
  char *buf = malloc(cap * elem_size);
  char *tmp = malloc(elem_size);

  while (it.next(&it, tmp)) {
    if (len == cap) {
      cap *= 2;
      buf = realloc(buf, cap * elem_size);
    }
    memcpy(buf + len * elem_size, tmp, elem_size);
    len++;
  }

  free(tmp);
  iter_drop(&it);

  if (len == 0) {
    free(buf);
    return (Slice){NULL, 0, elem_size};
  }

  void *out = arena_alloc(a, len * elem_size, _Alignof(max_align_t));
  memcpy(out, buf, len * elem_size);
  free(buf);
  return (Slice){out, len, elem_size};
}

size_t iter_count(Iter it) {
  size_t n = 0;
  void *tmp = malloc(it.elem_size);
  while (it.next(&it, tmp))
    n++;
  free(tmp);
  iter_drop(&it);
  return n;
}

void iter_foreach(Iter it, void (*fn)(const void *elem, void *ctx), void *ctx) {
  void *tmp = malloc(it.elem_size);
  while (it.next(&it, tmp))
    fn(tmp, ctx);
  free(tmp);
  iter_drop(&it);
}

void iter_reduce(Iter it, void *acc,
                 void (*combine)(void *acc, const void *elem, void *ctx),
                 void *ctx) {
  void *tmp = malloc(it.elem_size);
  while (it.next(&it, tmp))
    combine(acc, tmp, ctx);
  free(tmp);
  iter_drop(&it);
}
