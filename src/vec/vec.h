#pragma once

#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

typedef struct {
  void *data;
  size_t len;
  size_t cap;
  size_t elem_size;
  Allocator allocator;
} Vec;

Vec vec_create(size_t elem_size, Allocator allocator);
Vec vec_create_size(size_t elem_size, size_t capacity, Allocator allocator);
void vec_push(Vec *v, const void *elem);
void *vec_get(const Vec *v, size_t i);
Slice vec_as_slice(const Vec *v);
Iter vec_iter(const Vec *v);
void vec_free(Vec *v);
