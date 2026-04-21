#include <criterion/criterion.h>

#include "arena/arena.h"
#include "vec/vec.h"

Test(vec, create_is_empty) {
  Vec v = vec_create(sizeof(int), NULL);
  cr_assert_eq(v.len, 0);
  cr_assert_null(v.data);
  vec_free(&v);
}

Test(vec, push_increments_len) {
  Vec v = vec_create(sizeof(int), NULL);
  int x = 42;
  vec_push(&v, &x);
  cr_assert_eq(v.len, 1);
  vec_free(&v);
}

Test(vec, get_returns_pushed_value) {
  Vec v = vec_create(sizeof(int), NULL);
  int x = 99;
  vec_push(&v, &x);
  cr_assert_eq(*(int *)vec_get(&v, 0), 99);
  vec_free(&v);
}

Test(vec, push_many_preserves_values) {
  Vec v = vec_create(sizeof(int), NULL);
  for (int i = 0; i < 100; i++)
    vec_push(&v, &i);
  cr_assert_eq(v.len, 100);
  for (int i = 0; i < 100; i++)
    cr_assert_eq(*(int *)vec_get(&v, (size_t)i), i);
  vec_free(&v);
}

Test(vec, as_slice_reflects_contents) {
  Vec v = vec_create(sizeof(int), NULL);
  int x = 7, y = 8, z = 9;
  vec_push(&v, &x);
  vec_push(&v, &y);
  vec_push(&v, &z);

  Slice s = vec_as_slice(&v);
  cr_assert_eq(s.len, 3);
  cr_assert_eq(*(int *)slice_get(s, 1), 8);
  vec_free(&v);
}

Test(vec, iter_counts_all_elements) {
  Vec v = vec_create(sizeof(int), NULL);
  for (int i = 0; i < 5; i++)
    vec_push(&v, &i);

  cr_assert_eq(iter_count(vec_iter(&v)), 5);
  vec_free(&v);
}

Test(vec, iter_collect_round_trip) {
  Vec v = vec_create(sizeof(int), NULL);
  for (int i = 0; i < 4; i++)
    vec_push(&v, &i);

  Arena *a = arena_create(256);
  Slice result = iter_collect(vec_iter(&v), a);

  cr_assert_eq(result.len, 4);
  for (int i = 0; i < 4; i++)
    cr_assert_eq(*(int *)slice_get(result, (size_t)i), i);

  arena_free(a);
  vec_free(&v);
}
