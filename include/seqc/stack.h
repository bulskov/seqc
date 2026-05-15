#pragma once

#include "seqc/iter.h"

/* Stack — LIFO wrapper over Vec */
typedef struct Stack Stack;

Stack *stack_create(size_t elem_size, allocator_t allocator);
SeqcStatus stack_push(Stack *s, const void *elem);
SeqcStatus stack_pop(
    Stack *s, void *out);         /* SEQC_NOT_FOUND if empty; out may be NULL */
void *stack_peek(const Stack *s); /* pointer to top; NULL if empty */
bool stack_is_empty(const Stack *s);
size_t stack_len(const Stack *s);
Iter stack_iter(const Stack *s);     /* bottom→top */
Iter stack_iter_rev(const Stack *s); /* top→bottom */
void stack_clear(Stack *s);          /* empty the stack, keep buffer */
void stack_free(Stack *s);
