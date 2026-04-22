#include <criterion/criterion.h>

#include "arena/arena.h"
#include "queue/queue.h"

/* ---- tests ------------------------------------------------------------- */

Test(queue, is_empty_on_create) {
  Arena *a = arena_create(256);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  cr_assert(queue_is_empty(&q));
  cr_assert_eq(queue_len(&q), 0);
  arena_free(a);
}

Test(queue, push_pop_fifo_order) {
  Arena *a = arena_create(512);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  int vals[] = {1, 2, 3};
  for (int i = 0; i < 3; i++)
    queue_push(&q, &vals[i]);
  int out;
  cr_assert(queue_pop(&q, &out));
  cr_assert_eq(out, 1);
  cr_assert(queue_pop(&q, &out));
  cr_assert_eq(out, 2);
  cr_assert(queue_pop(&q, &out));
  cr_assert_eq(out, 3);
  cr_assert_not(queue_pop(&q, &out));
  arena_free(a);
}

Test(queue, peek_does_not_consume) {
  Arena *a = arena_create(256);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  int v = 99;
  queue_push(&q, &v);
  cr_assert_eq(*(int *)queue_peek(&q), 99);
  cr_assert_eq(queue_len(&q), 1);
  arena_free(a);
}

Test(queue, peek_empty_returns_null) {
  Arena *a = arena_create(64);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  cr_assert_null(queue_peek(&q));
  arena_free(a);
}

Test(queue, pop_empty_returns_0) {
  Arena *a = arena_create(64);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  int out;
  cr_assert_not(queue_pop(&q, &out));
  arena_free(a);
}

Test(queue, ring_wrap_around) {
  /* Push 16 elements (fills initial cap), pop 8, push 8 more — exercises
   * the ring-buffer wrap and triggers a resize on the 17th push. */
  Arena *a = arena_create(4096);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  for (int i = 0; i < 16; i++)
    queue_push(&q, &i);
  for (int i = 0; i < 8; i++) {
    int out;
    queue_pop(&q, &out);
    cr_assert_eq(out, i);
  }
  /* now head == 8 inside the ring buffer */
  for (int i = 16; i < 24; i++)
    queue_push(&q, &i);
  /* drain: expect 8,9,...,23 */
  for (int i = 8; i < 24; i++) {
    int out;
    cr_assert(queue_pop(&q, &out));
    cr_assert_eq(out, i);
  }
  cr_assert(queue_is_empty(&q));
  arena_free(a);
}

Test(queue, grow_beyond_initial_cap) {
  Arena *a = arena_create(4096);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  for (int i = 0; i < 32; i++)
    queue_push(&q, &i);
  cr_assert_eq(queue_len(&q), 32);
  for (int i = 0; i < 32; i++) {
    int out;
    cr_assert(queue_pop(&q, &out));
    cr_assert_eq(out, i);
  }
  arena_free(a);
}

Test(queue, iter_front_to_back) {
  Arena *a = arena_create(512);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  int vals[] = {10, 20, 30};
  for (int i = 0; i < 3; i++)
    queue_push(&q, &vals[i]);
  Scratch sc = arena_scratch_push(a);
  Iter it = queue_iter(&q);
  int got[3];
  size_t n = 0;
  while (it.next(&it, &got[n]))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 3);
  cr_assert_eq(got[0], 10);
  cr_assert_eq(got[1], 20);
  cr_assert_eq(got[2], 30);
  arena_scratch_pop(&sc);
  arena_free(a);
}

/* ---- queue_clear ------------------------------------------------------- */

Test(queue, clear_empties_queue) {
  Arena *a = arena_create(256);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  for (int i = 0; i < 4; i++)
    queue_push(&q, &i);
  queue_clear(&q);
  cr_assert(queue_is_empty(&q));
  cr_assert_eq(queue_len(&q), 0);
  arena_free(a);
}

Test(queue, clear_allows_reuse) {
  Arena *a = arena_create(256);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  for (int i = 0; i < 3; i++)
    queue_push(&q, &i);
  queue_clear(&q);
  int x = 42;
  queue_push(&q, &x);
  int out;
  cr_assert(queue_pop(&q, &out));
  cr_assert_eq(out, 42);
  cr_assert(queue_is_empty(&q));
  arena_free(a);
}

/* ---- queue_iter_rev ---------------------------------------------------- */

Test(queue, iter_rev_back_to_front) {
  Arena *a = arena_create(512);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  int vals[] = {10, 20, 30};
  for (int i = 0; i < 3; i++)
    queue_push(&q, &vals[i]);
  Scratch sc = arena_scratch_push(a);
  Iter it = queue_iter_rev(&q);
  int got[3];
  size_t n = 0;
  while (it.next(&it, &got[n]))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 3);
  cr_assert_eq(got[0], 30);
  cr_assert_eq(got[1], 20);
  cr_assert_eq(got[2], 10);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(queue, iter_rev_wraps_ring_buffer) {
  /* Push 5, pop 2 to shift head, then check rev order covers the wrap */
  Arena *a = arena_create(512);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  int vals[] = {1, 2, 3, 4, 5};
  for (int i = 0; i < 5; i++)
    queue_push(&q, &vals[i]);
  int discard;
  queue_pop(&q, &discard); /* remove 1 */
  queue_pop(&q, &discard); /* remove 2 */
  /* queue is now: 3 4 5 (front→back) */
  Scratch sc = arena_scratch_push(a);
  Iter it = queue_iter_rev(&q);
  int got[3];
  size_t n = 0;
  while (it.next(&it, &got[n]))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 3);
  cr_assert_eq(got[0], 5);
  cr_assert_eq(got[1], 4);
  cr_assert_eq(got[2], 3);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(queue, iter_rev_empty) {
  Arena *a = arena_create(256);
  Queue q = queue_create(sizeof(int), arena_allocator(a));
  Iter it = queue_iter_rev(&q);
  int v;
  cr_assert(!it.next(&it, &v));
  iter_drop(&it);
  arena_free(a);
}
