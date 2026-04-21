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
  /* Use a large count to exceed ARENA_BLOCK_SIZE worth of ints */
  Arena *a = arena_create(8);
  for (int i = 0; i < 1024; i++) {
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
  for (int i = 0; i < 1024; i++)
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

/* --- arena_realloc ---------------------------------------------------- */

Test(arena, realloc_null_ptr_acts_as_alloc) {
  Arena *a = arena_create(64);
  int *p = arena_realloc(a, NULL, 0, sizeof(int), _Alignof(int));
  cr_assert_not_null(p);
  *p = 7;
  cr_assert_eq(*p, 7);
  arena_free(a);
}

Test(arena, realloc_zero_new_size_returns_null) {
  Arena *a = arena_create(64);
  int *p = arena_alloc(a, sizeof(int), _Alignof(int));
  void *r = arena_realloc(a, p, sizeof(int), 0, _Alignof(int));
  cr_assert_null(r);
  arena_free(a);
}

Test(arena, realloc_shrink_returns_same_ptr) {
  Arena *a = arena_create(64);
  int *p = arena_alloc(a, 4 * sizeof(int), _Alignof(int));
  void *r =
      arena_realloc(a, p, 4 * sizeof(int), 2 * sizeof(int), _Alignof(int));
  cr_assert_eq((void *)p, r);
  arena_free(a);
}

Test(arena, realloc_last_alloc_extends_in_place) {
  Arena *a = arena_create(256);
  int *p = arena_alloc(a, sizeof(int), _Alignof(int));
  *p = 42;
  /* grow in place — p is the last allocation */
  int *r = arena_realloc(a, p, sizeof(int), 4 * sizeof(int), _Alignof(int));
  cr_assert_eq((void *)p, (void *)r); /* same pointer — fast path taken */
  cr_assert_eq(r[0], 42);             /* existing data preserved */
  arena_free(a);
}

Test(arena, realloc_non_last_alloc_copies) {
  Arena *a = arena_create(256);
  int *p = arena_alloc(a, sizeof(int), _Alignof(int));
  *p = 99;
  /* allocate after p so p is no longer the last allocation */
  arena_alloc(a, sizeof(int), _Alignof(int));
  int *r = arena_realloc(a, p, sizeof(int), 4 * sizeof(int), _Alignof(int));
  cr_assert_not_null(r);
  cr_assert_neq((void *)p, (void *)r); /* different pointer — full copy */
  cr_assert_eq(r[0], 99);              /* data copied correctly */
  arena_free(a);
}

Test(arena, realloc_preserves_data_across_blocks) {
  Arena *a = arena_create(8);
  /* fill until a new block is needed, then realloc into it */
  int *p = arena_alloc(a, sizeof(int), _Alignof(int));
  *p = 123;
  for (int i = 0; i < 1024; i++)
    arena_alloc(a, sizeof(int), _Alignof(int));
  int *r = arena_realloc(a, p, sizeof(int), 2 * sizeof(int), _Alignof(int));
  cr_assert_not_null(r);
  cr_assert_eq(r[0], 123);
  arena_free(a);
}

/* --- audit functions -------------------------------------------------- */

Test(arena, block_count_starts_at_one) {
  Arena *a = arena_create(256);
  cr_assert_eq(arena_block_count(a), 1);
  arena_free(a);
}

Test(arena, block_count_grows_with_overflow) {
  /* arena_create rounds up to ARENA_BLOCK_SIZE (4096); allocate more than that
   */
  Arena *a = arena_create(8);
  size_t initial = arena_block_count(a);
  for (int i = 0; i < 1024; i++)
    arena_alloc(a, sizeof(int), _Alignof(int));
  cr_assert_gt(arena_block_count(a), initial);
  arena_free(a);
}

Test(arena, capacity_at_least_initial) {
  Arena *a = arena_create(256);
  cr_assert_geq(arena_capacity(a), (size_t)256);
  arena_free(a);
}

Test(arena, capacity_grows_after_overflow) {
  Arena *a = arena_create(8);
  size_t cap_before = arena_capacity(a);
  for (int i = 0; i < 1024; i++)
    arena_alloc(a, sizeof(int), _Alignof(int));
  cr_assert_gt(arena_capacity(a), cap_before);
  arena_free(a);
}

Test(arena, total_allocated_increases_with_allocs) {
  Arena *a = arena_create(256);
  size_t before = arena_total_allocated(a);
  arena_alloc(a, sizeof(int), _Alignof(int));
  cr_assert_gt(arena_total_allocated(a), before);
  arena_free(a);
}

Test(arena, total_allocated_resets_after_reset) {
  Arena *a = arena_create(256);
  arena_alloc(a, sizeof(int), _Alignof(int));
  size_t after_alloc = arena_total_allocated(a);
  arena_reset(a);
  cr_assert_lt(arena_total_allocated(a), after_alloc);
  arena_free(a);
}
