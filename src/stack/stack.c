#include "stack.h"

#include <string.h>

Stack stack_create(size_t elem_size, Allocator allocator) {
  return (Stack){vec_create(elem_size, allocator)};
}

void stack_push(Stack *s, const void *elem) {
  if (!s)
    return;
  vec_push(&s->vec, elem);
}

bool stack_pop(Stack *s, void *out) {
  if (!s || s->vec.len == 0)
    return false;
  s->vec.len--;
  if (out)
    memcpy(out, (char *)s->vec.data + s->vec.len * s->vec.elem_size,
           s->vec.elem_size);
  return true;
}

void *stack_peek(const Stack *s) {
  if (!s || s->vec.len == 0)
    return NULL;
  return (char *)s->vec.data + (s->vec.len - 1) * s->vec.elem_size;
}

bool stack_is_empty(const Stack *s) { return !s || s->vec.len == 0; }

size_t stack_len(const Stack *s) { return s ? s->vec.len : 0; }

Iter stack_iter(const Stack *s) {
  if (!s)
    return (Iter){0};
  return vec_iter(&s->vec);
}

Iter stack_iter_rev(const Stack *s) {
  if (!s)
    return (Iter){0};
  return vec_iter_rev(&s->vec);
}

void stack_clear(Stack *s) {
  if (s)
    vec_clear(&s->vec);
}

void stack_free(Stack *s) {
  if (!s)
    return;
  vec_free(&s->vec);
}
