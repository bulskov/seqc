#include <criterion/criterion.h>

#include "arena/arena.h"
#include "dlist/dlist.h"

/* ---- tests ------------------------------------------------------------- */

Test(dlist, is_empty_on_create) {
  Arena *a = arena_create(256);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  cr_assert(dlist_is_empty(&l));
  cr_assert_eq(dlist_len(&l), 0);
  arena_free(a);
}

Test(dlist, push_back_iter_forward) {
  Arena *a = arena_create(512);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  int vals[] = {1, 2, 3};
  for (int i = 0; i < 3; i++)
    dlist_push_back(&l, &vals[i]);
  Iter it = dlist_iter(&l);
  int got[3];
  size_t n = 0;
  while (it.next(&it, &got[n]))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 3);
  cr_assert_eq(got[0], 1);
  cr_assert_eq(got[1], 2);
  cr_assert_eq(got[2], 3);
  arena_free(a);
}

Test(dlist, iter_reverse) {
  Arena *a = arena_create(512);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  int vals[] = {1, 2, 3};
  for (int i = 0; i < 3; i++)
    dlist_push_back(&l, &vals[i]);
  Iter it = dlist_iter_reverse(&l);
  int got[3];
  size_t n = 0;
  while (it.next(&it, &got[n]))
    n++;
  iter_drop(&it);
  cr_assert_eq(n, 3);
  cr_assert_eq(got[0], 3);
  cr_assert_eq(got[1], 2);
  cr_assert_eq(got[2], 1);
  arena_free(a);
}

Test(dlist, push_front_prepends) {
  Arena *a = arena_create(512);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  int two = 2, one = 1;
  dlist_push_back(&l, &two);
  dlist_push_front(&l, &one);
  cr_assert_eq(*(int *)dlist_front(&l), 1);
  cr_assert_eq(*(int *)dlist_back(&l), 2);
  arena_free(a);
}

Test(dlist, pop_front_removes_head) {
  Arena *a = arena_create(512);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  int vals[] = {10, 20, 30};
  for (int i = 0; i < 3; i++)
    dlist_push_back(&l, &vals[i]);
  int out;
  cr_assert(dlist_pop_front(&l, &out));
  cr_assert_eq(out, 10);
  cr_assert_eq(*(int *)dlist_front(&l), 20);
  cr_assert_eq(dlist_len(&l), 2);
  arena_free(a);
}

Test(dlist, pop_back_removes_tail) {
  Arena *a = arena_create(512);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  int vals[] = {10, 20, 30};
  for (int i = 0; i < 3; i++)
    dlist_push_back(&l, &vals[i]);
  int out;
  cr_assert(dlist_pop_back(&l, &out));
  cr_assert_eq(out, 30);
  cr_assert_eq(*(int *)dlist_back(&l), 20);
  cr_assert_eq(dlist_len(&l), 2);
  arena_free(a);
}

Test(dlist, pop_until_empty) {
  Arena *a = arena_create(256);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  int v = 1;
  dlist_push_back(&l, &v);
  int out;
  cr_assert(dlist_pop_front(&l, &out));
  cr_assert_not(dlist_pop_front(&l, &out));
  cr_assert(dlist_is_empty(&l));
  cr_assert_null(dlist_front(&l));
  cr_assert_null(dlist_back(&l));
  arena_free(a);
}

Test(dlist, front_back_null_if_empty) {
  Arena *a = arena_create(64);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  cr_assert_null(dlist_front(&l));
  cr_assert_null(dlist_back(&l));
  arena_free(a);
}

Test(dlist, single_element_front_equals_back) {
  Arena *a = arena_create(256);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  int v = 42;
  dlist_push_back(&l, &v);
  cr_assert_eq(*(int *)dlist_front(&l), 42);
  cr_assert_eq(*(int *)dlist_back(&l), 42);
  arena_free(a);
}

Test(dlist, prev_links_are_correct) {
  /* verify backward linkage by popping from the back repeatedly */
  Arena *a = arena_create(512);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  int vals[] = {1, 2, 3, 4, 5};
  for (int i = 0; i < 5; i++)
    dlist_push_back(&l, &vals[i]);
  for (int i = 4; i >= 0; i--) {
    int out;
    cr_assert(dlist_pop_back(&l, &out));
    cr_assert_eq(out, vals[i]);
  }
  cr_assert(dlist_is_empty(&l));
  arena_free(a);
}

Test(dlist, free_does_not_crash) {
  Arena *a = arena_create(512);
  DList l = dlist_create(sizeof(int), arena_allocator(a));
  int v = 1;
  dlist_push_back(&l, &v);
  dlist_push_back(&l, &v);
  dlist_free(&l);
  cr_assert(dlist_is_empty(&l));
  arena_free(a);
}
