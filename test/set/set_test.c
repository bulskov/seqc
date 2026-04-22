#include <criterion/criterion.h>

#include "arena/arena.h"
#include "set/set.h"

/* ---- hash/eq for int keys ---------------------------------------------- */

static size_t int_hash(const void *key, size_t key_size) {
  (void)key_size;
  /* Knuth multiplicative hash */
  return (size_t)(*(const unsigned int *)key) * 2654435761u;
}

static int int_eq(const void *a, const void *b, size_t key_size) {
  (void)key_size;
  return *(const int *)a == *(const int *)b;
}

/* ---- tests ------------------------------------------------------------- */

Test(set, is_empty_on_create) {
  Arena *a = arena_create(256);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  cr_assert_eq(set_len(&s), 0);
  arena_free(a);
}

Test(set, add_contains) {
  Arena *a = arena_create(1024);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  int v1 = 1, v2 = 2, v3 = 42;
  cr_assert(set_add(&s, &v1));
  cr_assert(set_add(&s, &v2));
  cr_assert(set_add(&s, &v3));
  cr_assert(set_contains(&s, &v1));
  cr_assert(set_contains(&s, &v2));
  cr_assert(set_contains(&s, &v3));
  cr_assert_eq(set_len(&s), 3);
  arena_free(a);
}

Test(set, add_duplicate_returns_0) {
  Arena *a = arena_create(512);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  int v = 7;
  cr_assert(set_add(&s, &v));
  cr_assert_not(set_add(&s, &v)); /* already present */
  cr_assert_eq(set_len(&s), 1);
  arena_free(a);
}

Test(set, remove_existing) {
  Arena *a = arena_create(512);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  int v = 5;
  set_add(&s, &v);
  cr_assert(set_remove(&s, &v));
  cr_assert_not(set_contains(&s, &v));
  cr_assert_eq(set_len(&s), 0);
  arena_free(a);
}

Test(set, remove_nonexistent_returns_0) {
  Arena *a = arena_create(256);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  int v = 99;
  cr_assert_not(set_remove(&s, &v));
  arena_free(a);
}

Test(set, does_not_contain_absent_key) {
  Arena *a = arena_create(512);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  int present = 1, absent = 2;
  set_add(&s, &present);
  cr_assert_not(set_contains(&s, &absent));
  arena_free(a);
}

Test(set, iter_yields_all_elements) {
  Arena *a = arena_create(1024);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  int vals[] = {10, 20, 30, 40};
  for (int i = 0; i < 4; i++)
    set_add(&s, &vals[i]);
  Scratch sc = arena_scratch_push(a);
  size_t count = iter_count(set_iter(&s));
  cr_assert_eq(count, 4);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(set, grow_beyond_initial_cap) {
  /* Add 20 elements to force a resize (load factor 0.75 of initial cap 16) */
  Arena *a = arena_create(4096);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  for (int i = 0; i < 20; i++)
    set_add(&s, &i);
  cr_assert_eq(set_len(&s), 20);
  for (int i = 0; i < 20; i++)
    cr_assert(set_contains(&s, &i));
  arena_free(a);
}

/* ---- set_clear --------------------------------------------------------- */

Test(set, clear_empties_set) {
  Arena *a = arena_create(1024);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  for (int i = 0; i < 5; i++)
    set_add(&s, &i);
  set_clear(&s);
  cr_assert_eq(set_len(&s), 0);
  for (int i = 0; i < 5; i++)
    cr_assert(!set_contains(&s, &i));
  arena_free(a);
}

Test(set, clear_allows_reuse) {
  Arena *a = arena_create(1024);
  Set s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
  for (int i = 0; i < 3; i++)
    set_add(&s, &i);
  set_clear(&s);
  int x = 42;
  cr_assert(set_add(&s, &x));
  cr_assert_eq(set_len(&s), 1);
  cr_assert(set_contains(&s, &x));
  arena_free(a);
}
