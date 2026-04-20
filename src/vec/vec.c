#include "vec.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 16

Vec vec_create(size_t elem_size) {
  return (Vec){.data = NULL, .len = 0, .cap = 0, .elem_size = elem_size};
}

void vec_push(Vec *v, const void *elem) {
  if (v->len == v->cap) {
    v->cap = v->cap == 0 ? INITIAL_CAP : v->cap * 2;
    v->data = realloc(v->data, v->cap * v->elem_size);
  }
  memcpy((char *)v->data + v->len * v->elem_size, elem, v->elem_size);
  v->len++;
}

void *vec_get(const Vec *v, size_t i) {
  assert(i < v->len);
  return (char *)v->data + i * v->elem_size;
}

Slice vec_as_slice(const Vec *v) {
  return (Slice){v->data, v->len, v->elem_size};
}

Iter vec_iter(const Vec *v) { return iter_from_slice(vec_as_slice(v)); }

void vec_free(Vec *v) {
  free(v->data);
  v->data = NULL;
  v->len = 0;
  v->cap = 0;
}
