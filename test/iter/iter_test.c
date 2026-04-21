#include <criterion/criterion.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* ---- helpers ----------------------------------------------------------- */

static int gt10(const void *elem, void *ctx) {
  (void)ctx;
  return *(const int *)elem > 10;
}

static void double_it(const void *in, void *out, void *ctx) {
  (void)ctx;
  *(int *)out = *(const int *)in * 2;
}

static void sum_combine(void *acc, const void *elem, void *ctx) {
  (void)ctx;
  *(int *)acc += *(const int *)elem;
}

static void push_to_arr(const void *elem, void *ctx) {
  int **p = ctx;
  *(*p)++ = *(const int *)elem;
}

/* ---- tests ------------------------------------------------------------- */

Test(iter, from_slice_count) {
  int data[] = {1, 2, 3, 4, 5};
  Slice s = {data, 5, sizeof(int)};
  cr_assert_eq(iter_count(iter_from_slice(s)), 5);
}

Test(iter, from_slice_empty) {
  Slice s = {NULL, 0, sizeof(int)};
  cr_assert_eq(iter_count(iter_from_slice(s)), 0);
}

Test(iter, filter_keeps_matching) {
  int data[] = {3, 15, 7, 22, 1, 18};
  Slice s = {data, 6, sizeof(int)};
  size_t n = iter_count(iter_filter(iter_from_slice(s), gt10, NULL));
  cr_assert_eq(n, 3);
}

Test(iter, filter_none_match) {
  int data[] = {1, 2, 3};
  Slice s = {data, 3, sizeof(int)};
  size_t n = iter_count(iter_filter(iter_from_slice(s), gt10, NULL));
  cr_assert_eq(n, 0);
}

Test(iter, map_doubles_values) {
  int data[] = {1, 2, 3};
  Slice s = {data, 3, sizeof(int)};
  Arena *a = arena_create(256);

  Slice result = iter_collect(
      iter_map(iter_from_slice(s), double_it, NULL, sizeof(int)), arena_allocator(a));

  cr_assert_eq(result.len, 3);
  cr_assert_eq(*(int *)slice_get(result, 0), 2);
  cr_assert_eq(*(int *)slice_get(result, 1), 4);
  cr_assert_eq(*(int *)slice_get(result, 2), 6);

  arena_free(a);
}

Test(iter, collect_produces_correct_slice) {
  int data[] = {10, 20, 30, 40};
  Slice s = {data, 4, sizeof(int)};
  Arena *a = arena_create(256);

  Slice result = iter_collect(iter_from_slice(s), arena_allocator(a));

  cr_assert_eq(result.len, 4);
  for (size_t i = 0; i < result.len; i++)
    cr_assert_eq(*(int *)slice_get(result, i), data[i]);

  arena_free(a);
}

Test(iter, collect_empty_gives_null_ptr) {
  Slice s = {NULL, 0, sizeof(int)};
  Arena *a = arena_create(64);

  Slice result = iter_collect(iter_from_slice(s), arena_allocator(a));

  cr_assert_eq(result.len, 0);
  cr_assert_null(result.ptr);

  arena_free(a);
}

Test(iter, take_limits_output) {
  int data[] = {1, 2, 3, 4, 5};
  Slice s = {data, 5, sizeof(int)};
  cr_assert_eq(iter_count(iter_take(iter_from_slice(s), 3)), 3);
}

Test(iter, take_more_than_available) {
  int data[] = {1, 2};
  Slice s = {data, 2, sizeof(int)};
  cr_assert_eq(iter_count(iter_take(iter_from_slice(s), 100)), 2);
}

Test(iter, skip_drops_first_n) {
  int data[] = {1, 2, 3, 4, 5};
  Slice s = {data, 5, sizeof(int)};
  cr_assert_eq(iter_count(iter_skip(iter_from_slice(s), 3)), 2);
}

Test(iter, skip_all) {
  int data[] = {1, 2, 3};
  Slice s = {data, 3, sizeof(int)};
  cr_assert_eq(iter_count(iter_skip(iter_from_slice(s), 10)), 0);
}

Test(iter, reduce_sum) {
  int data[] = {1, 2, 3, 4, 5};
  Slice s = {data, 5, sizeof(int)};
  int sum = 0;
  iter_reduce(iter_from_slice(s), &sum, sum_combine, NULL);
  cr_assert_eq(sum, 15);
}

Test(iter, foreach_visits_all) {
  int data[] = {10, 20, 30};
  Slice s = {data, 3, sizeof(int)};
  int out[3] = {0};
  int *ptr = out;
  iter_foreach(iter_from_slice(s), push_to_arr, &ptr);
  cr_assert_eq(out[0], 10);
  cr_assert_eq(out[1], 20);
  cr_assert_eq(out[2], 30);
}

Test(iter, filter_map_chain) {
  int data[] = {3, 15, 7, 22, 1, 18};
  Slice s = {data, 6, sizeof(int)};
  Arena *a = arena_create(256);

  Slice result =
      iter_collect(iter_map(iter_filter(iter_from_slice(s), gt10, NULL),
                            double_it, NULL, sizeof(int)),
                   arena_allocator(a));

  /* 15→30, 22→44, 18→36 */
  cr_assert_eq(result.len, 3);
  cr_assert_eq(*(int *)slice_get(result, 0), 30);
  cr_assert_eq(*(int *)slice_get(result, 1), 44);
  cr_assert_eq(*(int *)slice_get(result, 2), 36);

  arena_free(a);
}
