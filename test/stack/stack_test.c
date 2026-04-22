#include <criterion/criterion.h>

#include "arena/arena.h"
#include "stack/stack.h"

/* ---- helpers ----------------------------------------------------------- */

static void push_ints(Stack *s, const int *arr, size_t n) {
  for (size_t i = 0; i < n; i++)
    stack_push(s, &arr[i]);
}

/* ---- tests ------------------------------------------------------------- */

Test(stack, is_empty_on_create) {
  Arena *a = arena_create(256);
  Stack s = stack_create(sizeof(int), arena_allocator(a));
  cr_assert(stack_is_empty(&s));
  cr_assert_eq(stack_len(&s), 0);
  arena_free(a);
}

Test(stack, push_pop_lifo_order) {
  Arena *a = arena_create(512);
  Stack s = stack_create(sizeof(int), arena_allocator(a));
  int vals[] = {1, 2, 3};
  push_ints(&s, vals, 3);
  cr_assert_eq(stack_len(&s), 3);
  int out;
  cr_assert(stack_pop(&s, &out));
  cr_assert_eq(out, 3);
  cr_assert(stack_pop(&s, &out));
  cr_assert_eq(out, 2);
  cr_assert(stack_pop(&s, &out));
  cr_assert_eq(out, 1);
  cr_assert_not(stack_pop(&s, &out));
  arena_free(a);
}

Test(stack, peek_does_not_consume) {
  Arena *a = arena_create(256);
  Stack s = stack_create(sizeof(int), arena_allocator(a));
  int v = 42;
  stack_push(&s, &v);
  cr_assert_eq(*(int *)stack_peek(&s), 42);
  cr_assert_eq(stack_len(&s), 1); /* peek didn't pop */
  arena_free(a);
}

Test(stack, peek_empty_returns_null) {
  Arena *a = arena_create(64);
  Stack s = stack_create(sizeof(int), arena_allocator(a));
  cr_assert_null(stack_peek(&s));
  arena_free(a);
}

Test(stack, pop_empty_returns_0) {
  Arena *a = arena_create(64);
  Stack s = stack_create(sizeof(int), arena_allocator(a));
  int out;
  cr_assert_not(stack_pop(&s, &out));
  arena_free(a);
}

Test(stack, iter_bottom_to_top) {
  Arena *a = arena_create(512);
  Stack s = stack_create(sizeof(int), arena_allocator(a));
  int vals[] = {10, 20, 30};
  push_ints(&s, vals, 3);
  /* iter goes bottom→top: 10, 20, 30 */
  Iter it = stack_iter(&s);
  int got[3];
  size_t i = 0;
  while (it.next(&it, &got[i]))
    i++;
  iter_drop(&it);
  cr_assert_eq(i, 3);
  cr_assert_eq(got[0], 10);
  cr_assert_eq(got[1], 20);
  cr_assert_eq(got[2], 30);
  arena_free(a);
}

Test(stack, pop_null_out_ok) {
  Arena *a = arena_create(256);
  Stack s = stack_create(sizeof(int), arena_allocator(a));
  int v = 7;
  stack_push(&s, &v);
  cr_assert(stack_pop(&s, NULL)); /* just discard */
  cr_assert(stack_is_empty(&s));
  arena_free(a);
}
