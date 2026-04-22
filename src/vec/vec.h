#pragma once

#include <stdbool.h>
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
bool vec_pop(Vec *v, void *out); /* out may be NULL; false if empty */
void *vec_get(const Vec *v, size_t i);
void vec_set(Vec *v, size_t i, const void *elem); /* overwrite element at i */
void vec_insert(Vec *v, size_t i, const void *elem); /* shift [i..] right */
void vec_remove(Vec *v, size_t i);                   /* shift [i+1..] left */
void vec_reserve(Vec *v, size_t capacity); /* ensure cap >= capacity */
Slice vec_as_slice(const Vec *v);
Iter vec_iter(const Vec *v);
Iter vec_iter_rev(const Vec *v);
void vec_clear(Vec *v); /* reset len to 0, keep buffer */
void vec_free(Vec *v);
