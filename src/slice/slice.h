#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  void *ptr;
  size_t len;
  size_t elem_size;
} Slice;

void *slice_get(Slice s, size_t i);

/* Linear search. pred(elem, ctx) must return true to match.
 * Returns pointer to the first matching element, or NULL. */
void *slice_find(Slice s, bool (*pred)(const void *elem, void *ctx), void *ctx);
bool  slice_contains(Slice s, bool (*pred)(const void *elem, void *ctx), void *ctx);
