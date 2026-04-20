#include <criterion/criterion.h>

#include "arena/arena.h"

Test(arena, create_and_free) {
  Arena *a = arena_create(64);
  cr_assert_not_null(a);
  arena_free(a);
}

Test(arena, alloc_returns_non_null) {
  Arena *a = arena_create(64);
  void *p = arena_alloc(a, sizeof(int), _Alignof(int));
  cr_assert_not_null(p);
  arena_free(a);
}

Test(arena, alloc_is_writable) {
  Arena *a = arena_create(64);
  int *x = arena_alloc(a, sizeof(int), _Alignof(int));
  *x = 42;
  cr_assert_eq(*x, 42);
  arena_free(a);
}

Test(arena, alignment_respected) {
  Arena *a = arena_create(128);
  /* Force a misaligned position then request aligned alloc */
  arena_alloc(a, 1, 1);
  int *x = arena_alloc(a, sizeof(int), _Alignof(int));
  cr_assert_eq((size_t)x % _Alignof(int), 0);
  arena_free(a);
}

Test(arena, grows_beyond_initial_capacity) {
  Arena *a = arena_create(8);
  for (int i = 0; i < 64; i++) {
    int *p = arena_alloc(a, sizeof(int), _Alignof(int));
    cr_assert_not_null(p);
    *p = i;
  }
  arena_free(a);
}

Test(arena, reset_reuses_memory) {
  Arena *a = arena_create(64);
  int *x = arena_alloc(a, sizeof(int), _Alignof(int));
  *x = 99;
  arena_reset(a);
  int *y = arena_alloc(a, sizeof(int), _Alignof(int));
  cr_assert_eq(x, y);
  arena_free(a);
}

Test(arena, alloc_zero_returns_null) {
  Arena *a = arena_create(64);
  void *p = arena_alloc(a, 0, 1);
  cr_assert_null(p);
  arena_free(a);
}
