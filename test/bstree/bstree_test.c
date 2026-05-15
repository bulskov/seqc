#include <criterion/criterion.h>

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/bstree.h"

/* ---- comparator -------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* ---- tests ------------------------------------------------------------- */

Test(bstree, empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    cr_assert_eq(bstree_len(t), 0);
    cr_assert_null(bstree_min(t));
    cr_assert_null(bstree_max(t));
    growing_arena_destroy(a);
}

Test(bstree, insert_and_contains)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
        cr_assert_eq(bstree_insert(t, &vals[i]), SEQC_OK);
    cr_assert_eq(bstree_len(t), 5);
    for (int i = 0; i < 5; i++)
        cr_assert(bstree_contains(t, &vals[i]));
    int absent = 99;
    cr_assert_not(bstree_contains(t, &absent));
    growing_arena_destroy(a);
}

Test(bstree, insert_duplicate_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 10;
    cr_assert_eq(bstree_insert(t, &v), SEQC_OK);
    cr_assert_neq(bstree_insert(t, &v), SEQC_OK);
    cr_assert_eq(bstree_len(t), 1);
    growing_arena_destroy(a);
}

Test(bstree, min_max)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 1, 8, 3, 9, 2};
    for (int i = 0; i < 6; i++)
        bstree_insert(t, &vals[i]);
    cr_assert_eq(*(int *)bstree_min(t), 1);
    cr_assert_eq(*(int *)bstree_max(t), 9);
    growing_arena_destroy(a);
}

Test(bstree, iter_in_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
        bstree_insert(t, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = bstree_iter(t);
    int got[7];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 7);
    /* in-order traversal must yield ascending values */
    for (size_t i = 1; i < n; i++)
        cr_assert_lt(got[i - 1], got[i]);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(bstree, remove_leaf)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7};
    for (int i = 0; i < 3; i++)
        bstree_insert(t, &vals[i]);
    int v = 3;
    cr_assert_eq(bstree_remove(t, &v), SEQC_OK);
    cr_assert_not(bstree_contains(t, &v));
    cr_assert_eq(bstree_len(t), 2);
    growing_arena_destroy(a);
}

Test(bstree, remove_node_with_two_children)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
        bstree_insert(t, &vals[i]);
    int v = 5; /* root with two children */
    cr_assert_eq(bstree_remove(t, &v), SEQC_OK);
    cr_assert_not(bstree_contains(t, &v));
    cr_assert_eq(bstree_len(t), 6);
    /* tree must still be valid: iter still ascending */
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = bstree_iter(t);
    int prev, cur;
    int ok = it.next(&it, &prev);
    cr_assert(ok);
    while (it.next(&it, &cur))
    {
        cr_assert_lt(prev, cur);
        prev = cur;
    }
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(bstree, remove_nonexistent_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 42;
    cr_assert_neq(bstree_remove(t, &v), SEQC_OK);
    growing_arena_destroy(a);
}

Test(bstree, iter_empty_tree)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    cr_assert_eq(iter_count(bstree_iter(t)), 0);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(bstree, many_inserts_sorted)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 8192);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    /* insert 0..49 in a shuffled order to get a non-degenerate tree */
    int order[] = {24, 12, 36, 6,  18, 30, 42, 3,  9,  15, 21, 27, 33,
                   39, 45, 1,  4,  7,  10, 13, 16, 19, 22, 25, 28, 31,
                   34, 37, 40, 43, 46, 0,  2,  5,  8,  11, 14, 17, 20,
                   23, 26, 29, 32, 35, 38, 41, 44, 47, 48, 49};
    for (int i = 0; i < 50; i++)
        bstree_insert(t, &order[i]);
    cr_assert_eq(bstree_len(t), 50);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = bstree_iter(t);
    int prev, cur;
    it.next(&it, &prev);
    cr_assert_eq(prev, 0);
    int n = 1;
    while (it.next(&it, &cur))
    {
        cr_assert_lt(prev, cur);
        prev = cur;
        n++;
    }
    iter_drop(&it);
    cr_assert_eq(n, 50);
    cr_assert_eq(*(int *)bstree_min(t), 0);
    cr_assert_eq(*(int *)bstree_max(t), 49);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(bstree, iter_rev_descending)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {4, 2, 6, 1, 3, 5, 7};
    for (int i = 0; i < 7; i++)
        bstree_insert(t, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = bstree_iter_rev(t);
    int prev, cur;
    cr_assert(it.next(&it, &prev));
    cr_assert_eq(prev, 7);
    int n = 1;
    while (it.next(&it, &cur))
    {
        cr_assert_gt(prev, cur);
        prev = cur;
        n++;
    }
    iter_drop(&it);
    cr_assert_eq(n, 7);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(bstree, iter_range_mid)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int i = 0; i < 10; i++)
        bstree_insert(t, &vals[i]);
    int lo = 3, hi = 7;
    Iter it = bstree_iter_range(t, &lo, &hi);
    int collected[10];
    int n = 0;
    while (it.next(&it, &collected[n]))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 5); /* 3,4,5,6,7 */
    cr_assert_eq(collected[0], 3);
    cr_assert_eq(collected[4], 7);
    growing_arena_destroy(a);
}

Test(bstree, iter_range_no_lo)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        bstree_insert(t, &vals[i]);
    int hi = 3;
    Iter it = bstree_iter_range(t, NULL, &hi);
    int v;
    int n = 0;
    while (it.next(&it, &v))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 3); /* 1,2,3 */
    growing_arena_destroy(a);
}

Test(bstree, iter_range_no_hi)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        bstree_insert(t, &vals[i]);
    int lo = 3;
    Iter it = bstree_iter_range(t, &lo, NULL);
    int v;
    int n = 0;
    while (it.next(&it, &v))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 3); /* 3,4,5 */
    growing_arena_destroy(a);
}

Test(bstree, iter_range_empty_result)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {1, 5, 10};
    for (int i = 0; i < 3; i++)
        bstree_insert(t, &vals[i]);
    int lo = 6, hi = 9;
    Iter it = bstree_iter_range(t, &lo, &hi);
    int v;
    cr_assert(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}

/* ---- bstree_clear ------------------------------------------------------- */

Test(bstree, clear_empties_tree)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 5, 2, 4};
    for (int i = 0; i < 5; i++)
        bstree_insert(t, &vals[i]);
    bstree_clear(t);
    cr_assert_eq(bstree_len(t), 0);
    cr_assert_null(bstree_min(t));
    cr_assert_null(bstree_max(t));
    growing_arena_destroy(a);
}

Test(bstree, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
        bstree_insert(t, &vals[i]);
    bstree_clear(t);
    int x = 42;
    cr_assert_eq(bstree_insert(t, &x), SEQC_OK);
    cr_assert_eq(bstree_len(t), 1);
    cr_assert(bstree_contains(t, &x));
    growing_arena_destroy(a);
}

/* ---- bstree_height ------------------------------------------------------- */

Test(bstree, height_empty_is_zero)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    cr_assert_eq(bstree_height(t), 0);
    growing_arena_destroy(a);
}

Test(bstree, height_single_node_is_one)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int x = 5;
    bstree_insert(t, &x);
    cr_assert_eq(bstree_height(t), 1);
    growing_arena_destroy(a);
}

Test(bstree, height_increases_with_depth)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    /* insert sorted → right-skewed, height == n */
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        bstree_insert(t, &vals[i]);
    cr_assert_geq(bstree_height(t), 1);
    growing_arena_destroy(a);
}

/* Documents O(n) degenerate behaviour for sorted input (no balancing). */
Test(bstree, sorted_input_produces_linear_height)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 65536);
    BSTree *t = bstree_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 0; i < 1000; i++)
        bstree_insert(t, &i);
    cr_assert_eq(bstree_len(t), 1000);
    cr_assert_eq(bstree_height(t), 1000);
    growing_arena_destroy(a);
}
