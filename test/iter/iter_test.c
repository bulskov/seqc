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
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  cr_assert_eq(iter_count(iter_from_slice(s, scratch_allocator(&sc))), 5);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, from_slice_empty) {
  Slice s = {NULL, 0, sizeof(int)};
  Arena *a = arena_create(64);
  Scratch sc = arena_scratch_push(a);
  cr_assert_eq(iter_count(iter_from_slice(s, scratch_allocator(&sc))), 0);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, filter_keeps_matching) {
  int data[] = {3, 15, 7, 22, 1, 18};
  Slice s = {data, 6, sizeof(int)};
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  size_t n = iter_count(
      iter_filter(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
  cr_assert_eq(n, 3);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, filter_none_match) {
  int data[] = {1, 2, 3};
  Slice s = {data, 3, sizeof(int)};
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  size_t n = iter_count(
      iter_filter(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
  cr_assert_eq(n, 0);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, map_doubles_values) {
  int data[] = {1, 2, 3};
  Slice s = {data, 3, sizeof(int)};
  Arena *a = arena_create(256);

  Slice result = iter_collect(iter_map(iter_from_slice(s, arena_allocator(a)),
                                       double_it, NULL, sizeof(int)));

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

  Slice result = iter_collect(iter_from_slice(s, arena_allocator(a)));

  cr_assert_eq(result.len, 4);
  for (size_t i = 0; i < result.len; i++)
    cr_assert_eq(*(int *)slice_get(result, i), data[i]);

  arena_free(a);
}

Test(iter, collect_empty_gives_null_ptr) {
  Slice s = {NULL, 0, sizeof(int)};
  Arena *a = arena_create(64);

  Slice result = iter_collect(iter_from_slice(s, arena_allocator(a)));

  cr_assert_eq(result.len, 0);
  cr_assert_null(result.ptr);

  arena_free(a);
}

Test(iter, take_limits_output) {
  int data[] = {1, 2, 3, 4, 5};
  Slice s = {data, 5, sizeof(int)};
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  cr_assert_eq(
      iter_count(iter_take(iter_from_slice(s, scratch_allocator(&sc)), 3)), 3);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, take_more_than_available) {
  int data[] = {1, 2};
  Slice s = {data, 2, sizeof(int)};
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  cr_assert_eq(
      iter_count(iter_take(iter_from_slice(s, scratch_allocator(&sc)), 100)),
      2);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, skip_drops_first_n) {
  int data[] = {1, 2, 3, 4, 5};
  Slice s = {data, 5, sizeof(int)};
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  cr_assert_eq(
      iter_count(iter_skip(iter_from_slice(s, scratch_allocator(&sc)), 3)), 2);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, skip_all) {
  int data[] = {1, 2, 3};
  Slice s = {data, 3, sizeof(int)};
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  cr_assert_eq(
      iter_count(iter_skip(iter_from_slice(s, scratch_allocator(&sc)), 10)), 0);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, reduce_sum) {
  int data[] = {1, 2, 3, 4, 5};
  Slice s = {data, 5, sizeof(int)};
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  int sum = 0;
  iter_reduce(iter_from_slice(s, scratch_allocator(&sc)), &sum, sum_combine,
              NULL);
  cr_assert_eq(sum, 15);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, foreach_visits_all) {
  int data[] = {10, 20, 30};
  Slice s = {data, 3, sizeof(int)};
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  int out[3] = {0};
  int *ptr = out;
  iter_foreach(iter_from_slice(s, scratch_allocator(&sc)), push_to_arr, &ptr);
  cr_assert_eq(out[0], 10);
  cr_assert_eq(out[1], 20);
  cr_assert_eq(out[2], 30);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(iter, filter_map_chain) {
  int data[] = {3, 15, 7, 22, 1, 18};
  Slice s = {data, 6, sizeof(int)};
  Arena *a = arena_create(256);

  Slice result = iter_collect(
      iter_map(iter_filter(iter_from_slice(s, arena_allocator(a)), gt10, NULL),
               double_it, NULL, sizeof(int)));

  /* 15→30, 22→44, 18→36 */
  cr_assert_eq(result.len, 3);
  cr_assert_eq(*(int *)slice_get(result, 0), 30);
  cr_assert_eq(*(int *)slice_get(result, 1), 44);
  cr_assert_eq(*(int *)slice_get(result, 2), 36);

  arena_free(a);
}

/* ---- iter_chain -------------------------------------------------------- */

Test(iter, chain_concatenates) {
  int a[] = {1, 2, 3};
  int b[] = {4, 5};
  Slice sa = {a, 3, sizeof(int)};
  Slice sb = {b, 2, sizeof(int)};
  Arena *arena = arena_create(512);
  Scratch sc = arena_scratch_push(arena);
  Allocator al = scratch_allocator(&sc);
  size_t n =
      iter_count(iter_chain(iter_from_slice(sa, al), iter_from_slice(sb, al)));
  cr_assert_eq(n, 5);
  arena_scratch_pop(&sc);
  arena_free(arena);
}

Test(iter, chain_collects_in_order) {
  int a[] = {1, 2};
  int b[] = {3, 4};
  Slice sa = {a, 2, sizeof(int)};
  Slice sb = {b, 2, sizeof(int)};
  Arena *arena = arena_create(512);
  Allocator al = arena_allocator(arena);
  Slice result = iter_collect(
      iter_chain(iter_from_slice(sa, al), iter_from_slice(sb, al)));
  cr_assert_eq(result.len, 4);
  cr_assert_eq(*(int *)slice_get(result, 0), 1);
  cr_assert_eq(*(int *)slice_get(result, 3), 4);
  arena_free(arena);
}

Test(iter, chain_empty_first) {
  int b[] = {7, 8};
  Slice sa = {NULL, 0, sizeof(int)};
  Slice sb = {b, 2, sizeof(int)};
  Arena *arena = arena_create(256);
  Scratch sc = arena_scratch_push(arena);
  Allocator al = scratch_allocator(&sc);
  cr_assert_eq(
      iter_count(iter_chain(iter_from_slice(sa, al), iter_from_slice(sb, al))),
      2);
  arena_scratch_pop(&sc);
  arena_free(arena);
}

/* ---- iter_zip ---------------------------------------------------------- */

Test(iter, zip_pairs_elements) {
  int as[] = {1, 2, 3};
  char bs[] = {'a', 'b', 'c'};
  Slice sa = {as, 3, sizeof(int)};
  Slice sb = {bs, 3, sizeof(char)};
  Arena *arena = arena_create(512);
  Allocator al = arena_allocator(arena);
  Iter z = iter_zip(iter_from_slice(sa, al), iter_from_slice(sb, al));
  cr_assert_eq(z.elem_size, sizeof(int) + sizeof(char));
  char buf[sizeof(int) + sizeof(char)];
  int count = 0;
  while (z.next(&z, buf)) {
    int iv;
    memcpy(&iv, buf, sizeof(int));
    char cv;
    memcpy(&cv, buf + sizeof(int), sizeof(char));
    cr_assert_eq(iv, count + 1);
    cr_assert_eq(cv, 'a' + count);
    count++;
  }
  cr_assert_eq(count, 3);
  iter_drop(&z);
  arena_free(arena);
}

Test(iter, zip_stops_at_shorter) {
  int as[] = {1, 2, 3, 4};
  int bs[] = {10, 20};
  Slice sa = {as, 4, sizeof(int)};
  Slice sb = {bs, 2, sizeof(int)};
  Arena *arena = arena_create(512);
  Scratch sc = arena_scratch_push(arena);
  Allocator al = scratch_allocator(&sc);
  cr_assert_eq(
      iter_count(iter_zip(iter_from_slice(sa, al), iter_from_slice(sb, al))),
      2);
  arena_scratch_pop(&sc);
  arena_free(arena);
}

/* ---- iter_sort --------------------------------------------------------- */

static int int_cmp(const void *a, const void *b) {
  return *(const int *)a - *(const int *)b;
}

Test(iter, sort_ascending) {
  int data[] = {5, 1, 4, 2, 3};
  Slice s = {data, 5, sizeof(int)};
  Arena *arena = arena_create(512);
  Slice result = iter_sort(iter_from_slice(s, arena_allocator(arena)), int_cmp);
  cr_assert_eq(result.len, 5);
  for (size_t i = 0; i < result.len; i++)
    cr_assert_eq(*(int *)slice_get(result, i), (int)(i + 1));
  arena_free(arena);
}

Test(iter, sort_already_sorted) {
  int data[] = {1, 2, 3};
  Slice s = {data, 3, sizeof(int)};
  Arena *arena = arena_create(256);
  Slice result = iter_sort(iter_from_slice(s, arena_allocator(arena)), int_cmp);
  cr_assert_eq(*(int *)slice_get(result, 0), 1);
  cr_assert_eq(*(int *)slice_get(result, 2), 3);
  arena_free(arena);
}

/* ---- iter_find --------------------------------------------------------- */

Test(iter, find_returns_first_match) {
  int data[] = {1, 15, 22, 18};
  Slice s = {data, 4, sizeof(int)};
  Arena *arena = arena_create(256);
  Scratch sc = arena_scratch_push(arena);
  int out = 0;
  int found =
      iter_find(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL, &out);
  cr_assert(found);
  cr_assert_eq(out, 15);
  arena_scratch_pop(&sc);
  arena_free(arena);
}

Test(iter, find_not_found) {
  int data[] = {1, 2, 3};
  Slice s = {data, 3, sizeof(int)};
  Arena *arena = arena_create(256);
  Scratch sc = arena_scratch_push(arena);
  int found =
      iter_find(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL, NULL);
  cr_assert_not(found);
  arena_scratch_pop(&sc);
  arena_free(arena);
}

/* ---- iter_any / iter_all ----------------------------------------------- */

Test(iter, any_true_when_match_exists) {
  int data[] = {1, 2, 20};
  Slice s = {data, 3, sizeof(int)};
  Arena *arena = arena_create(256);
  Scratch sc = arena_scratch_push(arena);
  cr_assert(iter_any(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
  arena_scratch_pop(&sc);
  arena_free(arena);
}

Test(iter, any_false_when_no_match) {
  int data[] = {1, 2, 3};
  Slice s = {data, 3, sizeof(int)};
  Arena *arena = arena_create(256);
  Scratch sc = arena_scratch_push(arena);
  cr_assert_not(
      iter_any(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
  arena_scratch_pop(&sc);
  arena_free(arena);
}

Test(iter, all_true_when_all_match) {
  int data[] = {11, 22, 33};
  Slice s = {data, 3, sizeof(int)};
  Arena *arena = arena_create(256);
  Scratch sc = arena_scratch_push(arena);
  cr_assert(iter_all(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
  arena_scratch_pop(&sc);
  arena_free(arena);
}

Test(iter, all_false_when_one_fails) {
  int data[] = {11, 22, 5};
  Slice s = {data, 3, sizeof(int)};
  Arena *arena = arena_create(256);
  Scratch sc = arena_scratch_push(arena);
  cr_assert_not(
      iter_all(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
  arena_scratch_pop(&sc);
  arena_free(arena);
}

Test(iter, all_vacuously_true_for_empty) {
  Slice s = {NULL, 0, sizeof(int)};
  Arena *arena = arena_create(64);
  Scratch sc = arena_scratch_push(arena);
  cr_assert(iter_all(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
  arena_scratch_pop(&sc);
  arena_free(arena);
}

/* ---- iter_enumerate ---------------------------------------------------- */

Test(iter, enumerate_indices_and_values) {
  int data[] = {10, 20, 30};
  Slice s = {data, 3, sizeof(int)};
  Arena *arena = arena_create(256);
  Scratch sc = arena_scratch_push(arena);
  Iter it = iter_enumerate(iter_from_slice(s, scratch_allocator(&sc)));
  EnumEntry e;
  it.next(&it, &e);
  cr_assert_eq(e.index, 0);
  cr_assert_eq(*(int *)e.elem, 10);
  it.next(&it, &e);
  cr_assert_eq(e.index, 1);
  cr_assert_eq(*(int *)e.elem, 20);
  it.next(&it, &e);
  cr_assert_eq(e.index, 2);
  cr_assert_eq(*(int *)e.elem, 30);
  cr_assert_not(it.next(&it, &e));
  iter_drop(&it);
  arena_scratch_pop(&sc);
  arena_free(arena);
}

Test(iter, enumerate_empty) {
  Slice s = {NULL, 0, sizeof(int)};
  Arena *arena = arena_create(64);
  Scratch sc = arena_scratch_push(arena);
  Iter it = iter_enumerate(iter_from_slice(s, scratch_allocator(&sc)));
  EnumEntry e;
  cr_assert_not(it.next(&it, &e));
  iter_drop(&it);
  arena_scratch_pop(&sc);
  arena_free(arena);
}
