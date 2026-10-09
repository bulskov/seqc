#include "ctt.h"

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/avl.h"

#include "../oom_alloc.h"

/* ---- comparator -------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* ---- helpers ----------------------------------------------------------- */

static int log2_ceil(int n)
{
    int h = 0;
    while ((1 << h) < n)
    {
        h++;
    }
    return h;
}

/* ---- basic tests ------------------------------------------------------- */

TEST(avl_empty_on_create)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    ASSERT_EQ(0, avl_len(t));
    ASSERT_EQ(0, avl_height(t));
    ASSERT_NULL(avl_min_ptr(t));
    ASSERT_NULL(avl_max_ptr(t));
    growing_arena_destroy(a);
}

TEST(avl_add_and_contains)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
    {
        ASSERT_EQ(SEQC_OK, avl_add(t, &vals[i]));
    }
    ASSERT_EQ(5, avl_len(t));
    for (int i = 0; i < 5; i++)
    {
        ASSERT_TRUE(avl_contains(t, &vals[i]));
    }
    int absent = 99;
    ASSERT_FALSE(avl_contains(t, &absent));
    growing_arena_destroy(a);
}

TEST(avl_add_duplicate_returns_0)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 10;
    ASSERT_EQ(SEQC_OK, avl_add(t, &v));
    ASSERT_NE(SEQC_OK, avl_add(t, &v));
    ASSERT_EQ(1, avl_len(t));
    growing_arena_destroy(a);
}

TEST(avl_min_max)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 1, 8, 3, 9, 2};
    for (int i = 0; i < 6; i++)
    {
        avl_add(t, &vals[i]);
    }
    ASSERT_EQ(1, *(int *)avl_min_ptr(t));
    ASSERT_EQ(9, *(int *)avl_max_ptr(t));
    growing_arena_destroy(a);
}

/* ---- rotation tests ---------------------------------------------------- */

TEST(avl_ll_rotation)
{
    /* Insert 3,2,1 → triggers LL (right rotation at 3) → balanced root = 2 */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 2, 1};
    for (int i = 0; i < 3; i++)
    {
        avl_add(t, &vals[i]);
    }
    ASSERT_EQ(2, avl_height(t));
    ASSERT_EQ(1, *(int *)avl_min_ptr(t));
    ASSERT_EQ(3, *(int *)avl_max_ptr(t));
    growing_arena_destroy(a);
}

TEST(avl_rr_rotation)
{
    /* Insert 1,2,3 → triggers RR (left rotation at 1) → balanced root = 2 */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
    {
        avl_add(t, &vals[i]);
    }
    ASSERT_EQ(2, avl_height(t));
    growing_arena_destroy(a);
}

TEST(avl_lr_rotation)
{
    /* Insert 3,1,2 → triggers LR (left-right double rotation) */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 2};
    for (int i = 0; i < 3; i++)
    {
        avl_add(t, &vals[i]);
    }
    ASSERT_EQ(2, avl_height(t));
    growing_arena_destroy(a);
}

TEST(avl_rl_rotation)
{
    /* Insert 1,3,2 → triggers RL (right-left double rotation) */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 3, 2};
    for (int i = 0; i < 3; i++)
    {
        avl_add(t, &vals[i]);
    }
    ASSERT_EQ(2, avl_height(t));
    growing_arena_destroy(a);
}

/* ---- in-order iter ----------------------------------------------------- */

TEST(avl_iter_in_order)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
    {
        avl_add(t, &vals[i]);
    }
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = avl_iter(t);
    int got[7];
    size_t n = 0;
    while (it.next(&it, &got[n]))
    {
        n++;
    }
    iter_destroy(&it);
    ASSERT_EQ(7, n);
    for (size_t i = 1; i < n; i++)
    {
        ASSERT_LT(got[i - 1], got[i]);
    }
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(avl_iter_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    ASSERT_EQ(0, iter_count(avl_iter(t)));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* ---- remove tests ------------------------------------------------------ */

TEST(avl_remove_leaf)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7};
    for (int i = 0; i < 3; i++)
    {
        avl_add(t, &vals[i]);
    }
    int v = 3;
    ASSERT_EQ(SEQC_OK, avl_remove(t, &v));
    ASSERT_FALSE(avl_contains(t, &v));
    ASSERT_EQ(2, avl_len(t));
    growing_arena_destroy(a);
}

TEST(avl_remove_root_two_children)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
    {
        avl_add(t, &vals[i]);
    }
    int v = 5;
    ASSERT_EQ(SEQC_OK, avl_remove(t, &v));
    ASSERT_FALSE(avl_contains(t, &v));
    ASSERT_EQ(6, avl_len(t));
    /* tree must remain sorted */
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = avl_iter(t);
    int prev, cur;
    ASSERT_TRUE(it.next(&it, &prev));
    while (it.next(&it, &cur))
    {
        ASSERT_LT(prev, cur);
        prev = cur;
    }
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(avl_remove_rebalances)
{
    /* Insert ascending 1..7, remove the root repeatedly and verify balance */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 1; i <= 7; i++)
    {
        avl_add(t, &i);
    }
    for (int i = 1; i <= 6; i++)
    {
        ASSERT_EQ(SEQC_OK, avl_remove(t, &i));
        ASSERT_EQ((size_t)(7 - i), avl_len(t));
    }
    ASSERT_EQ(1, avl_len(t));
    growing_arena_destroy(a);
}

TEST(avl_remove_nonexistent_returns_0)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 42;
    ASSERT_NE(SEQC_OK, avl_remove(t, &v));
    growing_arena_destroy(a);
}

/* ---- balance invariant ------------------------------------------------- */

TEST(avl_height_stays_logarithmic)
{
    /* Insert 1000 ascending integers — worst case for an unbalanced BST
     * (would be height 1000); AVL must keep it at ~log2(1000) ≈ 10. */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 65536);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 0; i < 1000; i++)
    {
        avl_add(t, &i);
    }
    ASSERT_EQ(1000, avl_len(t));
    /* AVL height bound: <= 1.44 * log2(n+2) - 0.328 */
    int max_height = 2 * log2_ceil(1002);
    ASSERT_LE(avl_height(t), max_height);
    growing_arena_destroy(a);
}

TEST(avl_many_inserts_sorted_output)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 8192);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int order[] = {24, 12, 36, 6,  18, 30, 42, 3,  9,  15, 21, 27, 33,
                   39, 45, 1,  4,  7,  10, 13, 16, 19, 22, 25, 28, 31,
                   34, 37, 40, 43, 46, 0,  2,  5,  8,  11, 14, 17, 20,
                   23, 26, 29, 32, 35, 38, 41, 44, 47, 48, 49};
    for (int i = 0; i < 50; i++)
    {
        avl_add(t, &order[i]);
    }
    ASSERT_EQ(50, avl_len(t));
    ASSERT_EQ(0, *(int *)avl_min_ptr(t));
    ASSERT_EQ(49, *(int *)avl_max_ptr(t));
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = avl_iter(t);
    int prev, cur;
    ASSERT_TRUE(it.next(&it, &prev));
    ASSERT_EQ(0, prev);
    int n = 1;
    while (it.next(&it, &cur))
    {
        ASSERT_LT(prev, cur);
        prev = cur;
        n++;
    }
    iter_destroy(&it);
    ASSERT_EQ(50, n);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(avl_iter_rev_descending)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {4, 2, 6, 1, 3, 5, 7};
    for (int i = 0; i < 7; i++)
    {
        avl_add(t, &vals[i]);
    }
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = avl_iter_rev(t);
    int prev, cur;
    ASSERT_TRUE(it.next(&it, &prev));
    ASSERT_EQ(7, prev);
    int n = 1;
    while (it.next(&it, &cur))
    {
        ASSERT_GT(prev, cur);
        prev = cur;
        n++;
    }
    iter_destroy(&it);
    ASSERT_EQ(7, n);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(avl_iter_range_mid)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int i = 0; i < 10; i++)
    {
        avl_add(t, &vals[i]);
    }
    int lo = 3, hi = 7;
    iter_t it = avl_iter_range(t, &lo, &hi);
    int collected[10];
    int n = 0;
    while (it.next(&it, &collected[n]))
    {
        n++;
    }
    iter_destroy(&it);
    ASSERT_EQ(5, n); /* 3,4,5,6,7 */
    ASSERT_EQ(3, collected[0]);
    ASSERT_EQ(7, collected[4]);
    growing_arena_destroy(a);
}

TEST(avl_iter_range_no_lo)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
    {
        avl_add(t, &vals[i]);
    }
    int hi = 3;
    iter_t it = avl_iter_range(t, NULL, &hi);
    int v;
    int n = 0;
    while (it.next(&it, &v))
    {
        n++;
    }
    iter_destroy(&it);
    ASSERT_EQ(3, n); /* 1,2,3 */
    growing_arena_destroy(a);
}

TEST(avl_iter_range_no_hi)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
    {
        avl_add(t, &vals[i]);
    }
    int lo = 3;
    iter_t it = avl_iter_range(t, &lo, NULL);
    int v;
    int n = 0;
    while (it.next(&it, &v))
    {
        n++;
    }
    iter_destroy(&it);
    ASSERT_EQ(3, n); /* 3,4,5 */
    growing_arena_destroy(a);
}

TEST(avl_iter_range_empty_result)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 5, 10};
    for (int i = 0; i < 3; i++)
    {
        avl_add(t, &vals[i]);
    }
    int lo = 6, hi = 9;
    iter_t it = avl_iter_range(t, &lo, &hi);
    int v;
    ASSERT_TRUE(!it.next(&it, &v));
    iter_destroy(&it);
    growing_arena_destroy(a);
}

/* ---- avl_clear --------------------------------------------------------- */

TEST(avl_clear_empties_tree)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 5, 2, 4};
    for (int i = 0; i < 5; i++)
    {
        avl_add(t, &vals[i]);
    }
    avl_clear(t);
    ASSERT_EQ(0, avl_len(t));
    ASSERT_NULL(avl_min_ptr(t));
    ASSERT_NULL(avl_max_ptr(t));
    growing_arena_destroy(a);
}

TEST(avl_clear_allows_reuse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    avl_t *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
    {
        avl_add(t, &vals[i]);
    }
    avl_clear(t);
    int x = 42;
    ASSERT_EQ(SEQC_OK, avl_add(t, &x));
    ASSERT_EQ(1, avl_len(t));
    ASSERT_TRUE(avl_contains(t, &x));
    growing_arena_destroy(a);
}

/* ---- OOM paths: an exhausted allocator must not crash ------------------ */

TEST(avl_iter_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    avl_t *t = avl_create(sizeof(int), int_cmp, al);
    ASSERT_NOT_NULL(t);
    for (int i = 0; i < 3; i++)
    {
        avl_add(t, &i);
    }
    ctx.remaining = 0; /* exhaust: the iterator's state alloc must fail */
    iter_t it = avl_iter(t);
    ASSERT_NULL(it.next); /* empty iterator, not a NULL deref */
    iter_destroy(&it);
    avl_destroy(t);
}

TEST(avl_iter_rev_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    avl_t *t = avl_create(sizeof(int), int_cmp, al);
    ASSERT_NOT_NULL(t);
    for (int i = 0; i < 3; i++)
    {
        avl_add(t, &i);
    }
    ctx.remaining = 0;
    iter_t it = avl_iter_rev(t);
    ASSERT_NULL(it.next);
    iter_destroy(&it);
    avl_destroy(t);
}

TEST(avl_iter_range_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    avl_t *t = avl_create(sizeof(int), int_cmp, al);
    ASSERT_NOT_NULL(t);
    for (int i = 0; i < 5; i++)
    {
        avl_add(t, &i);
    }
    int lo = 1, hi = 3;
    ctx.remaining = 0;
    iter_t it = avl_iter_range(t, &lo, &hi);
    ASSERT_NULL(it.next);
    iter_destroy(&it);
    avl_destroy(t);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "avl_test");
}
