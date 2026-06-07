#pragma once

#include "seqc/iter.h"

/* seqc_stack_t — LIFO wrapper over vec_t.
 * Note the seqc_ prefix: POSIX reserves the bare name `stack_t` in <signal.h>
 * (sigaltstack), so this is the one type that cannot follow the plain
 * snake_case_t convention.  Do not rename it to stack_t. */
typedef struct seqc_stack_t seqc_stack_t;

seqc_stack_t *stack_create(size_t elem_size, allocator_t allocator);
seqc_status_t stack_push(seqc_stack_t *s, const void *elem);
seqc_status_t stack_pop(
    seqc_stack_t *s, void *out);         /* SEQC_NOT_FOUND if empty; out may be NULL */
void *stack_peek(const seqc_stack_t *s); /* pointer to top; NULL if empty */
bool stack_is_empty(const seqc_stack_t *s);
size_t stack_len(const seqc_stack_t *s);
iter_t stack_iter(const seqc_stack_t *s);     /* bottom→top */
iter_t stack_iter_rev(const seqc_stack_t *s); /* top→bottom */
void stack_clear(seqc_stack_t *s);          /* empty the stack, keep buffer */
void stack_free(seqc_stack_t *s);
