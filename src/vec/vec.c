#include "vec.h"
#include "arena/arena.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 16

Vec vec_create(size_t elem_size, Allocator allocator) {
  if (elem_size == 0 || !allocator.alloc || !allocator.realloc) {
    return (Vec){0};
  }

  return (Vec){.data = NULL,
               .len = 0,
               .cap = 0,
               .elem_size = elem_size,
               .allocator = allocator};
}

Vec vec_create_size(size_t elem_size, size_t capacity, Allocator allocator) {
  if (elem_size == 0 || capacity == 0 || !allocator.alloc ||
      !allocator.realloc) {
    return (Vec){0};
  }

  return (Vec){.data = allocator.alloc(allocator.ctx, capacity * elem_size,
                                       _Alignof(max_align_t)),
               .len = 0,
               .cap = capacity,
               .elem_size = elem_size,
               .allocator = allocator};
}

void vec_push(Vec *v, const void *elem) {
  if (!v || !elem) {
    return;
  }
  if (v->len == v->cap) {
    v->cap = v->cap == 0 ? INITIAL_CAP : v->cap * 2;
    v->data =
        v->allocator.realloc(v->allocator.ctx, v->data, v->len * v->elem_size,
                             v->cap * v->elem_size, _Alignof(max_align_t));
  }
  memcpy((char *)v->data + v->len * v->elem_size, elem, v->elem_size);
  v->len++;
}

void *vec_get(const Vec *v, size_t i) {
  if (!v || i >= v->len) {
    return NULL;
  }
  return (char *)v->data + i * v->elem_size;
}

Slice vec_as_slice(const Vec *v) {
  if (!v) {
    return (Slice){0};
  }
  return (Slice){v->data, v->len, v->elem_size};
}

Iter vec_iter(const Vec *v) {
  if (!v) {
    return (Iter){0};
  }
  return iter_from_slice(vec_as_slice(v), v->allocator);
}

void vec_free(Vec *v) {
  if (!v || !v->data) {
    return; /* nothing to free */
  }
  // real free is not needed since we are using an arena, but we null out the
  // fields

  if (v->allocator.free) {
    v->allocator.free(v->allocator.ctx, v->data);
  }

  v->allocator = (Allocator){0};
  v->data = NULL;
  v->len = 0;
  v->cap = 0;
}
