#include <criterion/criterion.h>

#include "arena/arena.h"
#include "pqueue/pqueue.h"

static int int_cmp(const void *a, const void *b) {
  return *(const int *)a - *(const int *)b;
}

/* negated comparator → max-heap */
static int int_cmp_rev(const void *a, const void *b) {
  return *(const int *)b - *(const int *)a;
}

Test(pqueue, empty_on_create) {
  Arena *a = arena_create(256);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  cr_assert_eq(pqueue_len(&q), 0);
  cr_assert(pqueue_is_empty(&q));
  cr_assert_null(pqueue_peek(&q));
  int out;
  cr_assert_not(pqueue_pop(&q, &out));
  arena_free(a);
}

Test(pqueue, push_and_peek) {
  Arena *a = arena_create(512);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {5, 3, 7, 1, 4};
  for (int i = 0; i < 5; i++)
    pqueue_push(&q, &vals[i]);
  cr_assert_eq(pqueue_len(&q), 5);
  cr_assert_eq(*(int *)pqueue_peek(&q), 1); /* min always at front */
  arena_free(a);
}

Test(pqueue, pop_yields_ascending_order) {
  Arena *a = arena_create(512);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6};
  for (int i = 0; i < 9; i++)
    pqueue_push(&q, &vals[i]);
  int prev, cur;
  cr_assert(pqueue_pop(&q, &prev));
  cr_assert_eq(prev, 1);
  for (int i = 1; i < 9; i++) {
    cr_assert(pqueue_pop(&q, &cur));
    cr_assert_leq(prev, cur);
    prev = cur;
  }
  cr_assert(pqueue_is_empty(&q));
  arena_free(a);
}

Test(pqueue, max_heap_via_reverse_cmp) {
  Arena *a = arena_create(512);
  PQueue q = pqueue_create(sizeof(int), int_cmp_rev, arena_allocator(a));
  int vals[] = {3, 1, 9, 5, 7};
  for (int i = 0; i < 5; i++)
    pqueue_push(&q, &vals[i]);
  cr_assert_eq(*(int *)pqueue_peek(&q), 9); /* max at front */
  int prev, cur;
  pqueue_pop(&q, &prev);
  while (pqueue_pop(&q, &cur)) {
    cr_assert_geq(prev, cur);
    prev = cur;
  }
  arena_free(a);
}

Test(pqueue, pop_discard_null_out) {
  Arena *a = arena_create(512);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {3, 1, 2};
  for (int i = 0; i < 3; i++)
    pqueue_push(&q, &vals[i]);
  cr_assert(pqueue_pop(&q, NULL)); /* discard min without crash */
  cr_assert_eq(pqueue_len(&q), 2);
  cr_assert_eq(*(int *)pqueue_peek(&q), 2);
  arena_free(a);
}

Test(pqueue, push_duplicates) {
  Arena *a = arena_create(512);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  int v = 5;
  pqueue_push(&q, &v);
  pqueue_push(&q, &v);
  pqueue_push(&q, &v);
  cr_assert_eq(pqueue_len(&q), 3);
  int out;
  pqueue_pop(&q, &out);
  cr_assert_eq(out, 5);
  pqueue_pop(&q, &out);
  cr_assert_eq(out, 5);
  pqueue_pop(&q, &out);
  cr_assert_eq(out, 5);
  cr_assert(pqueue_is_empty(&q));
  arena_free(a);
}

Test(pqueue, interleaved_push_pop) {
  Arena *a = arena_create(512);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  int v, out;
  v = 5;
  pqueue_push(&q, &v);
  v = 3;
  pqueue_push(&q, &v);
  cr_assert(pqueue_pop(&q, &out));
  cr_assert_eq(out, 3);
  v = 1;
  pqueue_push(&q, &v);
  v = 4;
  pqueue_push(&q, &v);
  cr_assert(pqueue_pop(&q, &out));
  cr_assert_eq(out, 1);
  cr_assert(pqueue_pop(&q, &out));
  cr_assert_eq(out, 4);
  cr_assert(pqueue_pop(&q, &out));
  cr_assert_eq(out, 5);
  cr_assert(pqueue_is_empty(&q));
  arena_free(a);
}
