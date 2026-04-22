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

/* ---- pqueue_clear ------------------------------------------------------ */

Test(pqueue, clear_empties_queue) {
  Arena *a = arena_create(256);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  for (int i = 5; i >= 1; i--)
    pqueue_push(&q, &i);
  pqueue_clear(&q);
  cr_assert(pqueue_is_empty(&q));
  cr_assert_eq(pqueue_len(&q), 0);
  arena_free(a);
}

Test(pqueue, clear_allows_reuse) {
  Arena *a = arena_create(256);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {5, 3, 7};
  for (int i = 0; i < 3; i++)
    pqueue_push(&q, &vals[i]);
  pqueue_clear(&q);
  int x = 42;
  pqueue_push(&q, &x);
  int out;
  cr_assert(pqueue_pop(&q, &out));
  cr_assert_eq(out, 42);
  cr_assert(pqueue_is_empty(&q));
  arena_free(a);
}

/* ---- pqueue_iter ------------------------------------------------------- */

Test(pqueue, iter_visits_all_elements) {
  Arena *a = arena_create(512);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {5, 1, 3, 2, 4};
  for (int i = 0; i < 5; i++)
    pqueue_push(&q, &vals[i]);
  /* Collect all elements via iter (order is heap-storage, not priority) */
  int seen[5];
  size_t n = 0;
  Iter it = pqueue_iter(&q);
  while (it.next(&it, &seen[n]))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 5);
  /* Verify all 5 values are present, regardless of order */
  int found[5] = {0};
  for (size_t i = 0; i < 5; i++)
    for (int v = 1; v <= 5; v++)
      if (seen[i] == v) {
        found[v - 1] = 1;
        break;
      }
  for (int i = 0; i < 5; i++)
    cr_assert(found[i]);
  /* queue is unchanged */
  cr_assert_eq(pqueue_len(&q), 5);
  arena_free(a);
}

Test(pqueue, iter_empty) {
  Arena *a = arena_create(256);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  Iter it = pqueue_iter(&q);
  int v;
  cr_assert(!it.next(&it, &v));
  iter_drop(&it);
  arena_free(a);
}

/* ---- pqueue_iter_rev --------------------------------------------------- */

Test(pqueue, iter_rev_visits_all_elements) {
  Arena *a = arena_create(512);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {5, 1, 3, 2, 4};
  for (int i = 0; i < 5; i++)
    pqueue_push(&q, &vals[i]);
  int seen_fwd[5], seen_rev[5];
  size_t nf = 0, nr = 0;
  Iter fwd = pqueue_iter(&q);
  while (fwd.next(&fwd, &seen_fwd[nf]))
    nf++;
  iter_drop(&fwd);
  Iter rev = pqueue_iter_rev(&q);
  while (rev.next(&rev, &seen_rev[nr]))
    nr++;
  iter_drop(&rev);
  cr_assert_eq(nf, 5);
  cr_assert_eq(nr, 5);
  /* rev must be exactly the reverse of fwd */
  for (size_t i = 0; i < 5; i++)
    cr_assert_eq(seen_rev[i], seen_fwd[4 - i]);
  arena_free(a);
}

Test(pqueue, iter_rev_empty) {
  Arena *a = arena_create(256);
  PQueue q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
  Iter it = pqueue_iter_rev(&q);
  int v;
  cr_assert(!it.next(&it, &v));
  iter_drop(&it);
  arena_free(a);
}
