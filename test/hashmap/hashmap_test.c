#include <criterion/criterion.h>

#include "arena/arena.h"
#include "hashmap/hashmap.h"
#include "iter/iter.h"

/* ---- string-keyed helpers ---- */
static HashMap make_str_map(Arena *a) {
  return hashmap_create(sizeof(char *), sizeof(int), hashmap_fnv1a_str,
                        hashmap_eq_str, arena_allocator(a));
}

Test(hashmap, str_set_and_get) {
  Arena *a = arena_create(4096);
  HashMap m = make_str_map(a);
  const char *k = "hello";
  int v = 123;
  hashmap_set(&m, &k, &v);
  int *got = hashmap_get(&m, &k);
  cr_assert_not_null(got);
  cr_assert_eq(*got, 123);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, str_distinct_keys) {
  Arena *a = arena_create(4096);
  HashMap m = make_str_map(a);
  const char *k1 = "foo";
  const char *k2 = "bar";
  int v1 = 1, v2 = 2;
  hashmap_set(&m, &k1, &v1);
  hashmap_set(&m, &k2, &v2);
  cr_assert_eq(*(int *)hashmap_get(&m, &k1), 1);
  cr_assert_eq(*(int *)hashmap_get(&m, &k2), 2);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, str_update_existing) {
  Arena *a = arena_create(4096);
  HashMap m = make_str_map(a);
  const char *k = "key";
  int v1 = 10, v2 = 99;
  hashmap_set(&m, &k, &v1);
  hashmap_set(&m, &k, &v2);
  cr_assert_eq(m.len, 1);
  cr_assert_eq(*(int *)hashmap_get(&m, &k), 99);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, str_delete) {
  Arena *a = arena_create(4096);
  HashMap m = make_str_map(a);
  const char *k = "gone";
  int v = 7;
  hashmap_set(&m, &k, &v);
  cr_assert_eq(hashmap_delete(&m, &k), 1);
  cr_assert_null(hashmap_get(&m, &k));
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, str_equal_content_different_pointer) {
  Arena *a = arena_create(4096);
  HashMap m = make_str_map(a);
  /* Two different pointers to the same content must match */
  char buf1[] = "same";
  char buf2[] = "same";
  const char *k1 = buf1;
  const char *k2 = buf2;
  int v = 42;
  hashmap_set(&m, &k1, &v);
  int *got = hashmap_get(&m, &k2);
  cr_assert_not_null(got);
  cr_assert_eq(*got, 42);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, create_is_empty) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  cr_assert_eq(m.len, 0);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, set_and_get) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  int k = 1, v = 42;
  hashmap_set(&m, &k, &v);
  int *got = hashmap_get(&m, &k);
  cr_assert_not_null(got);
  cr_assert_eq(*got, 42);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, get_missing_returns_null) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  int k = 99;
  cr_assert_null(hashmap_get(&m, &k));
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, set_updates_existing_key) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  int k = 1, v1 = 10, v2 = 20;
  hashmap_set(&m, &k, &v1);
  hashmap_set(&m, &k, &v2);
  cr_assert_eq(m.len, 1);
  cr_assert_eq(*(int *)hashmap_get(&m, &k), 20);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, delete_existing) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  int k = 7, v = 77;
  hashmap_set(&m, &k, &v);
  cr_assert_eq(hashmap_delete(&m, &k), 1);
  cr_assert_null(hashmap_get(&m, &k));
  cr_assert_eq(m.len, 0);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, delete_missing) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  int k = 5;
  cr_assert_eq(hashmap_delete(&m, &k), 0);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, delete_and_reinsert) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  int k = 3, v1 = 30, v2 = 300;
  hashmap_set(&m, &k, &v1);
  hashmap_delete(&m, &k);
  hashmap_set(&m, &k, &v2);
  cr_assert_eq(*(int *)hashmap_get(&m, &k), 300);
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, many_entries_triggers_resize) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  for (int i = 0; i < 100; i++) {
    int v = i * 10;
    hashmap_set(&m, &i, &v);
  }
  cr_assert_eq(m.len, 100);
  for (int i = 0; i < 100; i++) {
    int *got = hashmap_get(&m, &i);
    cr_assert_not_null(got);
    cr_assert_eq(*got, i * 10);
  }
  hashmap_free(&m);
  arena_free(a);
}

/* ---- iter integration -------------------------------------------------- */

static void sum_values(const void *elem, void *ctx) {
  const HashMapEntry *e = elem;
  *(int *)ctx += *(int *)e->value;
}

Test(hashmap, iter_foreach_sums_values) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  for (int i = 1; i <= 4; i++) {
    int v = i * 10;
    hashmap_set(&m, &i, &v);
  }
  int sum = 0;
  iter_foreach(hashmap_iter(&m), sum_values, &sum);
  cr_assert_eq(sum, 100); /* 10+20+30+40 */
  hashmap_free(&m);
  arena_free(a);
}

static bool value_gt_20(const void *elem, void *ctx) {
  (void)ctx;
  const HashMapEntry *e = elem;
  return *(int *)e->value > 20;
}

Test(hashmap, iter_filter_entries) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  for (int i = 1; i <= 4; i++) {
    int v = i * 10;
    hashmap_set(&m, &i, &v);
  }
  size_t count = iter_count(iter_filter(hashmap_iter(&m), value_gt_20, NULL));
  cr_assert_eq(count, 2); /* 30 and 40 */
  hashmap_free(&m);
  arena_free(a);
}

Test(hashmap, delete_middle_of_cluster) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  /* Insert several keys and delete one in the middle to exercise backward
   * shift */
  for (int i = 0; i < 10; i++) {
    int v = i;
    hashmap_set(&m, &i, &v);
  }
  int mid = 5;
  hashmap_delete(&m, &mid);
  cr_assert_null(hashmap_get(&m, &mid));
  for (int i = 0; i < 10; i++) {
    if (i == mid)
      continue;
    int *got = hashmap_get(&m, &i);
    cr_assert_not_null(got);
    cr_assert_eq(*got, i);
  }
  hashmap_free(&m);
  arena_free(a);
}

/* ---- hashmap_clear ----------------------------------------------------- */

Test(hashmap, clear_empties_map) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  for (int i = 0; i < 5; i++) {
    int v = i * 10;
    hashmap_set(&m, &i, &v);
  }
  hashmap_clear(&m);
  cr_assert_eq(hashmap_len(&m), 0);
  for (int i = 0; i < 5; i++)
    cr_assert_null(hashmap_get(&m, &i));
  arena_free(a);
}

Test(hashmap, clear_allows_reuse) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  for (int i = 0; i < 3; i++) {
    int v = i;
    hashmap_set(&m, &i, &v);
  }
  hashmap_clear(&m);
  int k = 99, v = 42;
  hashmap_set(&m, &k, &v);
  cr_assert_eq(hashmap_len(&m), 1);
  int *got = hashmap_get(&m, &k);
  cr_assert_not_null(got);
  cr_assert_eq(*got, 42);
  arena_free(a);
}

/* ---- hashmap_iter_rev -------------------------------------------------- */

Test(hashmap, iter_rev_visits_same_entries_in_reverse) {
  Arena *a = arena_create(4096);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  for (int i = 1; i <= 5; i++) {
    int v = i * 10;
    hashmap_set(&m, &i, &v);
  }
  /* Collect forward then reverse */
  HashMapEntry fwd[5], rev[5];
  size_t nf = 0, nr = 0;
  Iter it_fwd = hashmap_iter(&m);
  while (it_fwd.next(&it_fwd, &fwd[nf]))
    nf++;
  iter_drop(&it_fwd);
  Iter it_rev = hashmap_iter_rev(&m);
  while (it_rev.next(&it_rev, &rev[nr]))
    nr++;
  iter_drop(&it_rev);
  cr_assert_eq(nf, 5);
  cr_assert_eq(nr, 5);
  /* rev must be exactly the reverse of fwd */
  for (size_t i = 0; i < 5; i++) {
    cr_assert_eq(fwd[i].key, rev[4 - i].key);
    cr_assert_eq(fwd[i].value, rev[4 - i].value);
  }
  arena_free(a);
}

Test(hashmap, iter_rev_empty) {
  Arena *a = arena_create(256);
  HashMap m = hashmap_create(sizeof(int), sizeof(int), hashmap_fnv1a,
                             hashmap_eq_bytes, arena_allocator(a));
  Iter it = hashmap_iter_rev(&m);
  HashMapEntry e;
  cr_assert(!it.next(&it, &e));
  iter_drop(&it);
  hashmap_free(&m);
  arena_free(a);
}
