#include <criterion/criterion.h>

#include "arena/arena.h"
#include "btree/btree.h"

/* ---- comparator -------------------------------------------------------- */

static int int_cmp(const void *a, const void *b) {
  return *(const int *)a - *(const int *)b;
}

/* ---- tests ------------------------------------------------------------- */

Test(btree, empty_on_create) {
  Arena *a = arena_create(256);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  cr_assert_eq(btree_len(&t), 0);
  cr_assert_null(btree_min(&t));
  cr_assert_null(btree_max(&t));
  arena_free(a);
}

Test(btree, insert_and_contains) {
  Arena *a = arena_create(1024);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {5, 3, 7, 1, 4};
  for (int i = 0; i < 5; i++)
    cr_assert(btree_insert(&t, &vals[i]));
  cr_assert_eq(btree_len(&t), 5);
  for (int i = 0; i < 5; i++)
    cr_assert(btree_contains(&t, &vals[i]));
  int absent = 99;
  cr_assert_not(btree_contains(&t, &absent));
  arena_free(a);
}

Test(btree, insert_duplicate_returns_0) {
  Arena *a = arena_create(512);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int v = 10;
  cr_assert(btree_insert(&t, &v));
  cr_assert_not(btree_insert(&t, &v));
  cr_assert_eq(btree_len(&t), 1);
  arena_free(a);
}

Test(btree, min_max) {
  Arena *a = arena_create(512);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {5, 1, 8, 3, 9, 2};
  for (int i = 0; i < 6; i++)
    btree_insert(&t, &vals[i]);
  cr_assert_eq(*(int *)btree_min(&t), 1);
  cr_assert_eq(*(int *)btree_max(&t), 9);
  arena_free(a);
}

Test(btree, iter_in_order) {
  Arena *a = arena_create(1024);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {5, 3, 7, 1, 4, 6, 8};
  for (int i = 0; i < 7; i++)
    btree_insert(&t, &vals[i]);
  Scratch sc = arena_scratch_push(a);
  Iter it = btree_iter(&t);
  int got[7];
  size_t n = 0;
  while (it.next(&it, &got[n]))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 7);
  /* in-order traversal must yield ascending values */
  for (size_t i = 1; i < n; i++)
    cr_assert_lt(got[i - 1], got[i]);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(btree, remove_leaf) {
  Arena *a = arena_create(512);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {5, 3, 7};
  for (int i = 0; i < 3; i++)
    btree_insert(&t, &vals[i]);
  int v = 3;
  cr_assert(btree_remove(&t, &v));
  cr_assert_not(btree_contains(&t, &v));
  cr_assert_eq(btree_len(&t), 2);
  arena_free(a);
}

Test(btree, remove_node_with_two_children) {
  Arena *a = arena_create(1024);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {5, 3, 7, 1, 4, 6, 8};
  for (int i = 0; i < 7; i++)
    btree_insert(&t, &vals[i]);
  int v = 5; /* root with two children */
  cr_assert(btree_remove(&t, &v));
  cr_assert_not(btree_contains(&t, &v));
  cr_assert_eq(btree_len(&t), 6);
  /* tree must still be valid: iter still ascending */
  Scratch sc = arena_scratch_push(a);
  Iter it = btree_iter(&t);
  int prev, cur;
  int ok = it.next(&it, &prev);
  cr_assert(ok);
  while (it.next(&it, &cur)) {
    cr_assert_lt(prev, cur);
    prev = cur;
  }
  iter_drop(&it);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(btree, remove_nonexistent_returns_0) {
  Arena *a = arena_create(256);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int v = 42;
  cr_assert_not(btree_remove(&t, &v));
  arena_free(a);
}

Test(btree, iter_empty_tree) {
  Arena *a = arena_create(256);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  Scratch sc = arena_scratch_push(a);
  cr_assert_eq(iter_count(btree_iter(&t)), 0);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(btree, many_inserts_sorted) {
  Arena *a = arena_create(8192);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  /* insert 0..49 in a shuffled order to get a non-degenerate tree */
  int order[] = {24, 12, 36, 6,  18, 30, 42, 3,  9,  15, 21, 27, 33,
                 39, 45, 1,  4,  7,  10, 13, 16, 19, 22, 25, 28, 31,
                 34, 37, 40, 43, 46, 0,  2,  5,  8,  11, 14, 17, 20,
                 23, 26, 29, 32, 35, 38, 41, 44, 47, 48, 49};
  for (int i = 0; i < 50; i++)
    btree_insert(&t, &order[i]);
  cr_assert_eq(btree_len(&t), 50);
  Scratch sc = arena_scratch_push(a);
  Iter it = btree_iter(&t);
  int prev, cur;
  it.next(&it, &prev);
  cr_assert_eq(prev, 0);
  int n = 1;
  while (it.next(&it, &cur)) {
    cr_assert_lt(prev, cur);
    prev = cur;
    n++;
  }
  iter_drop(&it);
  cr_assert_eq(n, 50);
  cr_assert_eq(*(int *)btree_min(&t), 0);
  cr_assert_eq(*(int *)btree_max(&t), 49);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(btree, iter_rev_descending) {
  Arena *a = arena_create(1024);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {4, 2, 6, 1, 3, 5, 7};
  for (int i = 0; i < 7; i++)
    btree_insert(&t, &vals[i]);
  Scratch sc = arena_scratch_push(a);
  Iter it = btree_iter_rev(&t);
  int prev, cur;
  cr_assert(it.next(&it, &prev));
  cr_assert_eq(prev, 7);
  int n = 1;
  while (it.next(&it, &cur)) {
    cr_assert_gt(prev, cur);
    prev = cur;
    n++;
  }
  iter_drop(&it);
  cr_assert_eq(n, 7);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(btree, iter_range_mid) {
  Arena *a = arena_create(1024);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  for (int i = 0; i < 10; i++)
    btree_insert(&t, &vals[i]);
  int lo = 3, hi = 7;
  Iter it = btree_iter_range(&t, &lo, &hi);
  int collected[10];
  int n = 0;
  while (it.next(&it, &collected[n]))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 5); /* 3,4,5,6,7 */
  cr_assert_eq(collected[0], 3);
  cr_assert_eq(collected[4], 7);
  arena_free(a);
}

Test(btree, iter_range_no_lo) {
  Arena *a = arena_create(1024);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {1, 2, 3, 4, 5};
  for (int i = 0; i < 5; i++)
    btree_insert(&t, &vals[i]);
  int hi = 3;
  Iter it = btree_iter_range(&t, NULL, &hi);
  int v;
  int n = 0;
  while (it.next(&it, &v))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 3); /* 1,2,3 */
  arena_free(a);
}

Test(btree, iter_range_no_hi) {
  Arena *a = arena_create(1024);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {1, 2, 3, 4, 5};
  for (int i = 0; i < 5; i++)
    btree_insert(&t, &vals[i]);
  int lo = 3;
  Iter it = btree_iter_range(&t, &lo, NULL);
  int v;
  int n = 0;
  while (it.next(&it, &v))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 3); /* 3,4,5 */
  arena_free(a);
}

Test(btree, iter_range_empty_result) {
  Arena *a = arena_create(1024);
  BTree t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
  int vals[] = {1, 5, 10};
  for (int i = 0; i < 3; i++)
    btree_insert(&t, &vals[i]);
  int lo = 6, hi = 9;
  Iter it = btree_iter_range(&t, &lo, &hi);
  int v;
  cr_assert(!it.next(&it, &v));
  iter_drop(&it);
  arena_free(a);
}
