#include "ctt.h"

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/bstree.h"

#include "../oom_alloc.h"

/* ---- comparator -------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* ---- tests ------------------------------------------------------------- */

TEST(bstree_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    ASSERT_EQ(0, bstree_len(t));
    ASSERT_NULL(bstree_min(t));
    ASSERT_NULL(bstree_max(t));
    growing_arena_destroy(a);
}

TEST(bstree_insert_and_contains)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
        ASSERT_EQ(SEQC_OK, bstree_insert(t, &vals[i]));
    ASSERT_EQ(5, bstree_len(t));
    for (int i = 0; i < 5; i++)
        ASSERT_TRUE(bstree_contains(t, &vals[i]));
    int absent = 99;
    ASSERT_FALSE(bstree_contains(t, &absent));
    growing_arena_destroy(a);
}

TEST(bstree_insert_duplicate_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 10;
    ASSERT_EQ(SEQC_OK, bstree_insert(t, &v));
    ASSERT_NE(SEQC_OK, bstree_insert(t, &v));
    ASSERT_EQ(1, bstree_len(t));
    growing_arena_destroy(a);
}

TEST(bstree_min_max)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 1, 8, 3, 9, 2};
    for (int i = 0; i < 6; i++)
        bstree_insert(t, &vals[i]);
    ASSERT_EQ(1, *(int *)bstree_min(t));
    ASSERT_EQ(9, *(int *)bstree_max(t));
    growing_arena_destroy(a);
}

TEST(bstree_iter_in_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
        bstree_insert(t, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = bstree_iter(t);
    int got[7];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    ASSERT_EQ(7, n);
    /* in-order traversal must yield ascending values */
    for (size_t i = 1; i < n; i++)
        ASSERT_LT(got[i - 1], got[i]);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(bstree_remove_leaf)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7};
    for (int i = 0; i < 3; i++)
        bstree_insert(t, &vals[i]);
    int v = 3;
    ASSERT_EQ(SEQC_OK, bstree_remove(t, &v));
    ASSERT_FALSE(bstree_contains(t, &v));
    ASSERT_EQ(2, bstree_len(t));
    growing_arena_destroy(a);
}

TEST(bstree_remove_node_with_two_children)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
        bstree_insert(t, &vals[i]);
    int v = 5; /* root with two children */
    ASSERT_EQ(SEQC_OK, bstree_remove(t, &v));
    ASSERT_FALSE(bstree_contains(t, &v));
    ASSERT_EQ(6, bstree_len(t));
    /* tree must still be valid: iter still ascending */
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = bstree_iter(t);
    int prev, cur;
    int ok = it.next(&it, &prev);
    ASSERT_TRUE(ok);
    while (it.next(&it, &cur))
    {
        ASSERT_LT(prev, cur);
        prev = cur;
    }
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(bstree_remove_nonexistent_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 42;
    ASSERT_NE(SEQC_OK, bstree_remove(t, &v));
    growing_arena_destroy(a);
}

TEST(bstree_iter_empty_tree)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    ASSERT_EQ(0, iter_count(bstree_iter(t)));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(bstree_many_inserts_sorted)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 8192);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    /* insert 0..49 in a shuffled order to get a non-degenerate tree */
    int order[] = {24, 12, 36, 6,  18, 30, 42, 3,  9,  15, 21, 27, 33,
                   39, 45, 1,  4,  7,  10, 13, 16, 19, 22, 25, 28, 31,
                   34, 37, 40, 43, 46, 0,  2,  5,  8,  11, 14, 17, 20,
                   23, 26, 29, 32, 35, 38, 41, 44, 47, 48, 49};
    for (int i = 0; i < 50; i++)
        bstree_insert(t, &order[i]);
    ASSERT_EQ(50, bstree_len(t));
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = bstree_iter(t);
    int prev, cur;
    it.next(&it, &prev);
    ASSERT_EQ(0, prev);
    int n = 1;
    while (it.next(&it, &cur))
    {
        ASSERT_LT(prev, cur);
        prev = cur;
        n++;
    }
    iter_drop(&it);
    ASSERT_EQ(50, n);
    ASSERT_EQ(0, *(int *)bstree_min(t));
    ASSERT_EQ(49, *(int *)bstree_max(t));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(bstree_iter_rev_descending)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {4, 2, 6, 1, 3, 5, 7};
    for (int i = 0; i < 7; i++)
        bstree_insert(t, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = bstree_iter_rev(t);
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
    iter_drop(&it);
    ASSERT_EQ(7, n);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(bstree_iter_range_mid)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int i = 0; i < 10; i++)
        bstree_insert(t, &vals[i]);
    int lo = 3, hi = 7;
    iter_t it = bstree_iter_range(t, &lo, &hi);
    int collected[10];
    int n = 0;
    while (it.next(&it, &collected[n]))
        n++;
    iter_drop(&it);
    ASSERT_EQ(5, n); /* 3,4,5,6,7 */
    ASSERT_EQ(3, collected[0]);
    ASSERT_EQ(7, collected[4]);
    growing_arena_destroy(a);
}

TEST(bstree_iter_range_no_lo)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        bstree_insert(t, &vals[i]);
    int hi = 3;
    iter_t it = bstree_iter_range(t, NULL, &hi);
    int v;
    int n = 0;
    while (it.next(&it, &v))
        n++;
    iter_drop(&it);
    ASSERT_EQ(3, n); /* 1,2,3 */
    growing_arena_destroy(a);
}

TEST(bstree_iter_range_no_hi)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        bstree_insert(t, &vals[i]);
    int lo = 3;
    iter_t it = bstree_iter_range(t, &lo, NULL);
    int v;
    int n = 0;
    while (it.next(&it, &v))
        n++;
    iter_drop(&it);
    ASSERT_EQ(3, n); /* 3,4,5 */
    growing_arena_destroy(a);
}

TEST(bstree_iter_range_empty_result)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 5, 10};
    for (int i = 0; i < 3; i++)
        bstree_insert(t, &vals[i]);
    int lo = 6, hi = 9;
    iter_t it = bstree_iter_range(t, &lo, &hi);
    int v;
    ASSERT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}

/* ---- bstree_clear ------------------------------------------------------- */

TEST(bstree_clear_empties_tree)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 5, 2, 4};
    for (int i = 0; i < 5; i++)
        bstree_insert(t, &vals[i]);
    bstree_clear(t);
    ASSERT_EQ(0, bstree_len(t));
    ASSERT_NULL(bstree_min(t));
    ASSERT_NULL(bstree_max(t));
    growing_arena_destroy(a);
}

TEST(bstree_clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
        bstree_insert(t, &vals[i]);
    bstree_clear(t);
    int x = 42;
    ASSERT_EQ(SEQC_OK, bstree_insert(t, &x));
    ASSERT_EQ(1, bstree_len(t));
    ASSERT_TRUE(bstree_contains(t, &x));
    growing_arena_destroy(a);
}

/* ---- bstree_height ------------------------------------------------------- */

TEST(bstree_height_empty_is_zero)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    ASSERT_EQ(0, bstree_height(t));
    growing_arena_destroy(a);
}

TEST(bstree_height_single_node_is_one)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int x = 5;
    bstree_insert(t, &x);
    ASSERT_EQ(1, bstree_height(t));
    growing_arena_destroy(a);
}

TEST(bstree_height_increases_with_depth)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    /* insert sorted → right-skewed, height == n */
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        bstree_insert(t, &vals[i]);
    ASSERT_GE(bstree_height(t), 1);
    growing_arena_destroy(a);
}

/* Documents O(n) degenerate behaviour for sorted input (no balancing). */
TEST(bstree_sorted_input_produces_linear_height)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 65536);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 0; i < 1000; i++)
        bstree_insert(t, &i);
    ASSERT_EQ(1000, bstree_len(t));
    ASSERT_EQ(1000, bstree_height(t));
    growing_arena_destroy(a);
}

/* ---- OOM paths: an exhausted allocator must not crash ------------------ */

TEST(bstree_iter_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, al);
    ASSERT_NOT_NULL(t);
    for (int i = 0; i < 3; i++)
        bstree_insert(t, &i);
    ctx.remaining = 0; /* exhaust: the iterator's state alloc must fail */
    iter_t it = bstree_iter(t);
    ASSERT_NULL(it.next); /* empty iterator, not a NULL deref */
    iter_drop(&it);
    bstree_free(t);
}

TEST(bstree_iter_rev_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, al);
    ASSERT_NOT_NULL(t);
    for (int i = 0; i < 3; i++)
        bstree_insert(t, &i);
    ctx.remaining = 0;
    iter_t it = bstree_iter_rev(t);
    ASSERT_NULL(it.next);
    iter_drop(&it);
    bstree_free(t);
}

TEST(bstree_iter_range_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    bstree_t *t = bstree_create(sizeof(int), int_cmp, al);
    ASSERT_NOT_NULL(t);
    for (int i = 0; i < 5; i++)
        bstree_insert(t, &i);
    int lo = 1, hi = 3;
    ctx.remaining = 0;
    iter_t it = bstree_iter_range(t, &lo, &hi);
    ASSERT_NULL(it.next);
    iter_drop(&it);
    bstree_free(t);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "bstree_test");
}
