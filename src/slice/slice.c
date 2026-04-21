#include "slice.h"

void *slice_get(Slice s, size_t i) {
  if (s.ptr == NULL || s.elem_size == 0 || i >= s.len) {
    return NULL;
  }
  return (char *)s.ptr + i * s.elem_size;
}
