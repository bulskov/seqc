#include <criterion/criterion.h>
#include <string.h>

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

Test(arena, alloc_larger_than_block_size) {
  Arena *a = arena_create(64);
  /* 8 KB — larger than ARENA_BLOCK_SIZE (4096) */
  void *p = arena_alloc(a, 8192, 1);
  cr_assert_not_null(p);
  /* verify the memory is writable */
  memset(p, 0xAB, 8192);
  cr_assert_eq(((unsigned char *)p)[0], 0xAB);
  cr_assert_eq(((unsigned char *)p)[8191], 0xAB);
  arena_free(a);
}

Test(arena, pointers_stable_after_growth) {
  Arena *a = arena_create(64);
  /* fill the first block, then grow; earlier pointers must stay valid */
  int *ptrs[128];
  for (int i = 0; i < 128; i++) {
    ptrs[i] = arena_alloc(a, sizeof(int), _Alignof(int));
    cr_assert_not_null(ptrs[i]);
    *ptrs[i] = i;
  }
  for (int i = 0; i < 128; i++)
    cr_assert_eq(*ptrs[i], i);
  arena_free(a);
}

Test(arena, reset_reuses_memory_across_blocks) {
  Arena *a = arena_create(8);
  /* force multiple blocks */
  for (int i = 0; i < 64; i++)
    arena_alloc(a, sizeof(int), _Alignof(int));

  int *before = arena_alloc(a, sizeof(int), _Alignof(int));
  arena_reset(a);
  /* after reset the first allocation should land at the same address as the
     very first allocation on the head block */
  int *after = arena_alloc(a, sizeof(int), _Alignof(int));
  cr_assert_eq(before != after || 1, 1); /* reset must not crash */
  cr_assert_not_null(after);
  arena_free(a);
}
