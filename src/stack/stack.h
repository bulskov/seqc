#pragma once

#include "iter/iter.h"
#include "vec/vec.h"

/* Stack — LIFO wrapper over Vec */
typedef struct {
  Vec vec;
} Stack;

Stack stack_create(size_t elem_size, Allocator allocator);
void stack_push(Stack *s, const void *elem);
bool stack_pop(Stack *s, void *out); /* 1=ok 0=empty; out may be NULL */
void *stack_peek(const Stack *s);    /* pointer to top; NULL if empty */
bool stack_is_empty(const Stack *s);
size_t stack_len(const Stack *s);
Iter stack_iter(const Stack *s); /* bottom→top */
void stack_clear(Stack *s); /* empty the stack, keep buffer */
void stack_free(Stack *s);
