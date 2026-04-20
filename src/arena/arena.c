#include "arena.h"

#include <assert.h>
#include <stdlib.h>

struct Arena {
  char *buf;
  size_t cap;
  size_t pos;
};

Arena *arena_create(size_t capacity) {
  assert(capacity > 0);
  Arena *a = malloc(sizeof *a);
  a->buf = malloc(capacity);
  a->cap = capacity;
  a->pos = 0;
  return a;
}

void *arena_alloc(Arena *a, size_t size, size_t align) {
  if (size == 0)
    return NULL;
  if (align == 0)
    align = 1;

  size_t pad = (align - (a->pos % align)) % align;
  size_t needed = a->pos + pad + size;

  if (needed > a->cap) {
    size_t new_cap = a->cap * 2;
    while (new_cap < needed)
      new_cap *= 2;
    a->buf = realloc(a->buf, new_cap);
    a->cap = new_cap;
  }

  void *ptr = a->buf + a->pos + pad;
  a->pos = a->pos + pad + size;
  return ptr;
}

void arena_reset(Arena *a) { a->pos = 0; }

void arena_free(Arena *a) {
  free(a->buf);
  free(a);
}
