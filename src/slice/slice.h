#pragma once

#include <stddef.h>

typedef struct {
  void *ptr;
  size_t len;
  size_t elem_size;
} Slice;

void *slice_get(Slice s, size_t i);
