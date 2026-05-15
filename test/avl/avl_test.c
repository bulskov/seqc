#include <gtest/gtest.h>

extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/avl.h"
}

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
        h++;
    return h;
}

/* ---- basic tests ------------------------------------------------------- */

TEST(avl, empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    EXPECT_EQ(avl_len(t), 0);
    EXPECT_EQ(avl_height(t), 0);
    EXPECT_EQ(avl_min(t), nullptr);
    EXPECT_EQ(avl_max(t), nullptr);
    growing_arena_destroy(a);
}

TEST(avl, insert_and_contains)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
        EXPECT_EQ(avl_insert(t, &vals[i]), SEQC_OK);
    EXPECT_EQ(avl_len(t), 5);
    for (int i = 0; i < 5; i++)
        EXPECT_TRUE(avl_contains(t, &vals[i]));
    int absent = 99;
    EXPECT_FALSE(avl_contains(t, &absent));
    growing_arena_destroy(a);
}

TEST(avl, insert_duplicate_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 10;
    EXPECT_EQ(avl_insert(t, &v), SEQC_OK);
    EXPECT_NE(avl_insert(t, &v), SEQC_OK);
    EXPECT_EQ(avl_len(t), 1);
    growing_arena_destroy(a);
}

TEST(avl, min_max)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 1, 8, 3, 9, 2};
    for (int i = 0; i < 6; i++)
        avl_insert(t, &vals[i]);
    EXPECT_EQ(*(int *)avl_min(t), 1);
    EXPECT_EQ(*(int *)avl_max(t), 9);
    growing_arena_destroy(a);
}

/* ---- rotation tests ---------------------------------------------------- */

TEST(avl, ll_rotation)
{
    /* Insert 3,2,1 → triggers LL (right rotation at 3) → balanced root = 2 */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 2, 1};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    EXPECT_EQ(avl_height(t), 2);
    EXPECT_EQ(*(int *)avl_min(t), 1);
    EXPECT_EQ(*(int *)avl_max(t), 3);
    growing_arena_destroy(a);
}

TEST(avl, rr_rotation)
{
    /* Insert 1,2,3 → triggers RR (left rotation at 1) → balanced root = 2 */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    EXPECT_EQ(avl_height(t), 2);
    growing_arena_destroy(a);
}

TEST(avl, lr_rotation)
{
    /* Insert 3,1,2 → triggers LR (left-right double rotation) */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 2};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    EXPECT_EQ(avl_height(t), 2);
    growing_arena_destroy(a);
}

TEST(avl, rl_rotation)
{
    /* Insert 1,3,2 → triggers RL (right-left double rotation) */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 3, 2};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    EXPECT_EQ(avl_height(t), 2);
    growing_arena_destroy(a);
}

/* ---- in-order iter ----------------------------------------------------- */

TEST(avl, iter_in_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
        avl_insert(t, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = avl_iter(t);
    int got[7];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 7);
    for (size_t i = 1; i < n; i++)
        EXPECT_LT(got[i - 1], got[i]);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(avl, iter_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    EXPECT_EQ(iter_count(avl_iter(t)), 0);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* ---- remove tests ------------------------------------------------------ */

TEST(avl, remove_leaf)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    int v = 3;
    EXPECT_EQ(avl_remove(t, &v), SEQC_OK);
    EXPECT_FALSE(avl_contains(t, &v));
    EXPECT_EQ(avl_len(t), 2);
    growing_arena_destroy(a);
}

TEST(avl, remove_root_two_children)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
        avl_insert(t, &vals[i]);
    int v = 5;
    EXPECT_EQ(avl_remove(t, &v), SEQC_OK);
    EXPECT_FALSE(avl_contains(t, &v));
    EXPECT_EQ(avl_len(t), 6);
    /* tree must remain sorted */
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = avl_iter(t);
    int prev, cur;
    EXPECT_TRUE(it.next(&it, &prev));
    while (it.next(&it, &cur))
    {
        EXPECT_LT(prev, cur);
        prev = cur;
    }
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(avl, remove_rebalances)
{
    /* Insert ascending 1..7, remove the root repeatedly and verify balance */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 1; i <= 7; i++)
        avl_insert(t, &i);
    for (int i = 1; i <= 6; i++)
    {
        EXPECT_EQ(avl_remove(t, &i), SEQC_OK);
        EXPECT_EQ(avl_len(t), (size_t)(7 - i));
    }
    EXPECT_EQ(avl_len(t), 1);
    growing_arena_destroy(a);
}

TEST(avl, remove_nonexistent_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 42;
    EXPECT_NE(avl_remove(t, &v), SEQC_OK);
    growing_arena_destroy(a);
}

/* ---- balance invariant ------------------------------------------------- */

TEST(avl, height_stays_logarithmic)
{
    /* Insert 1000 ascending integers — worst case for an unbalanced BST
     * (would be height 1000); AVL must keep it at ~log2(1000) ≈ 10. */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 65536);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 0; i < 1000; i++)
        avl_insert(t, &i);
    EXPECT_EQ(avl_len(t), 1000);
    /* AVL height bound: <= 1.44 * log2(n+2) - 0.328 */
    int max_height = 2 * log2_ceil(1002);
    EXPECT_LE(avl_height(t), max_height);
    growing_arena_destroy(a);
}

TEST(avl, many_inserts_sorted_output)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 8192);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int order[] = {24, 12, 36, 6,  18, 30, 42, 3,  9,  15, 21, 27, 33,
                   39, 45, 1,  4,  7,  10, 13, 16, 19, 22, 25, 28, 31,
                   34, 37, 40, 43, 46, 0,  2,  5,  8,  11, 14, 17, 20,
                   23, 26, 29, 32, 35, 38, 41, 44, 47, 48, 49};
    for (int i = 0; i < 50; i++)
        avl_insert(t, &order[i]);
    EXPECT_EQ(avl_len(t), 50);
    EXPECT_EQ(*(int *)avl_min(t), 0);
    EXPECT_EQ(*(int *)avl_max(t), 49);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = avl_iter(t);
    int prev, cur;
    EXPECT_TRUE(it.next(&it, &prev));
    EXPECT_EQ(prev, 0);
    int n = 1;
    while (it.next(&it, &cur))
    {
        EXPECT_LT(prev, cur);
        prev = cur;
        n++;
    }
    iter_drop(&it);
    EXPECT_EQ(n, 50);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(avl, iter_rev_descending)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {4, 2, 6, 1, 3, 5, 7};
    for (int i = 0; i < 7; i++)
        avl_insert(t, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = avl_iter_rev(t);
    int prev, cur;
    EXPECT_TRUE(it.next(&it, &prev));
    EXPECT_EQ(prev, 7);
    int n = 1;
    while (it.next(&it, &cur))
    {
        EXPECT_GT(prev, cur);
        prev = cur;
        n++;
    }
    iter_drop(&it);
    EXPECT_EQ(n, 7);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(avl, iter_range_mid)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int i = 0; i < 10; i++)
        avl_insert(t, &vals[i]);
    int lo = 3, hi = 7;
    Iter it = avl_iter_range(t, &lo, &hi);
    int collected[10];
    int n = 0;
    while (it.next(&it, &collected[n]))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 5); /* 3,4,5,6,7 */
    EXPECT_EQ(collected[0], 3);
    EXPECT_EQ(collected[4], 7);
    growing_arena_destroy(a);
}

TEST(avl, iter_range_no_lo)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        avl_insert(t, &vals[i]);
    int hi = 3;
    Iter it = avl_iter_range(t, NULL, &hi);
    int v;
    int n = 0;
    while (it.next(&it, &v))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 3); /* 1,2,3 */
    growing_arena_destroy(a);
}

TEST(avl, iter_range_no_hi)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        avl_insert(t, &vals[i]);
    int lo = 3;
    Iter it = avl_iter_range(t, &lo, NULL);
    int v;
    int n = 0;
    while (it.next(&it, &v))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 3); /* 3,4,5 */
    growing_arena_destroy(a);
}

TEST(avl, iter_range_empty_result)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 5, 10};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    int lo = 6, hi = 9;
    Iter it = avl_iter_range(t, &lo, &hi);
    int v;
    EXPECT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}

/* ---- avl_clear --------------------------------------------------------- */

TEST(avl, clear_empties_tree)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 5, 2, 4};
    for (int i = 0; i < 5; i++)
        avl_insert(t, &vals[i]);
    avl_clear(t);
    EXPECT_EQ(avl_len(t), 0);
    EXPECT_EQ(avl_min(t), nullptr);
    EXPECT_EQ(avl_max(t), nullptr);
    growing_arena_destroy(a);
}

TEST(avl, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    avl_clear(t);
    int x = 42;
    EXPECT_EQ(avl_insert(t, &x), SEQC_OK);
    EXPECT_EQ(avl_len(t), 1);
    EXPECT_TRUE(avl_contains(t, &x));
    growing_arena_destroy(a);
}
