#include "slice.h"

#include <assert.h>

void *slice_get(Slice s, size_t i) {
  assert(i < s.len);
  return (char *)s.ptr + i * s.elem_size;
}
