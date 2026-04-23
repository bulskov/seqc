#include <criterion/criterion.h>

#include "arena/arena.h"
#include "avl/avl.h"

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

Test(avl, empty_on_create)
{
    Arena *a = arena_create(256);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    cr_assert_eq(avl_len(t), 0);
    cr_assert_eq(avl_height(t), 0);
    cr_assert_null(avl_min(t));
    cr_assert_null(avl_max(t));
    arena_free(a);
}

Test(avl, insert_and_contains)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
        cr_assert_eq(avl_insert(t, &vals[i]), SEQC_OK);
    cr_assert_eq(avl_len(t), 5);
    for (int i = 0; i < 5; i++)
        cr_assert(avl_contains(t, &vals[i]));
    int absent = 99;
    cr_assert_not(avl_contains(t, &absent));
    arena_free(a);
}

Test(avl, insert_duplicate_returns_0)
{
    Arena *a = arena_create(512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int v = 10;
    cr_assert_eq(avl_insert(t, &v), SEQC_OK);
    cr_assert_neq(avl_insert(t, &v), SEQC_OK);
    cr_assert_eq(avl_len(t), 1);
    arena_free(a);
}

Test(avl, min_max)
{
    Arena *a = arena_create(512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {5, 1, 8, 3, 9, 2};
    for (int i = 0; i < 6; i++)
        avl_insert(t, &vals[i]);
    cr_assert_eq(*(int *)avl_min(t), 1);
    cr_assert_eq(*(int *)avl_max(t), 9);
    arena_free(a);
}

/* ---- rotation tests ---------------------------------------------------- */

Test(avl, ll_rotation)
{
    /* Insert 3,2,1 → triggers LL (right rotation at 3) → balanced root = 2 */
    Arena *a = arena_create(512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {3, 2, 1};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    cr_assert_eq(avl_height(t), 2);
    cr_assert_eq(*(int *)avl_min(t), 1);
    cr_assert_eq(*(int *)avl_max(t), 3);
    arena_free(a);
}

Test(avl, rr_rotation)
{
    /* Insert 1,2,3 → triggers RR (left rotation at 1) → balanced root = 2 */
    Arena *a = arena_create(512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    cr_assert_eq(avl_height(t), 2);
    arena_free(a);
}

Test(avl, lr_rotation)
{
    /* Insert 3,1,2 → triggers LR (left-right double rotation) */
    Arena *a = arena_create(512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {3, 1, 2};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    cr_assert_eq(avl_height(t), 2);
    arena_free(a);
}

Test(avl, rl_rotation)
{
    /* Insert 1,3,2 → triggers RL (right-left double rotation) */
    Arena *a = arena_create(512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {1, 3, 2};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    cr_assert_eq(avl_height(t), 2);
    arena_free(a);
}

/* ---- in-order iter ----------------------------------------------------- */

Test(avl, iter_in_order)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
        avl_insert(t, &vals[i]);
    Scratch sc = arena_scratch_push(a);
    Iter it = avl_iter(t);
    int got[7];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 7);
    for (size_t i = 1; i < n; i++)
        cr_assert_lt(got[i - 1], got[i]);
    arena_scratch_pop(&sc);
    arena_free(a);
}

Test(avl, iter_empty)
{
    Arena *a = arena_create(256);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    Scratch sc = arena_scratch_push(a);
    cr_assert_eq(iter_count(avl_iter(t)), 0);
    arena_scratch_pop(&sc);
    arena_free(a);
}

/* ---- remove tests ------------------------------------------------------ */

Test(avl, remove_leaf)
{
    Arena *a = arena_create(512);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {5, 3, 7};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    int v = 3;
    cr_assert_eq(avl_remove(t, &v), SEQC_OK);
    cr_assert_not(avl_contains(t, &v));
    cr_assert_eq(avl_len(t), 2);
    arena_free(a);
}

Test(avl, remove_root_two_children)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
        avl_insert(t, &vals[i]);
    int v = 5;
    cr_assert_eq(avl_remove(t, &v), SEQC_OK);
    cr_assert_not(avl_contains(t, &v));
    cr_assert_eq(avl_len(t), 6);
    /* tree must remain sorted */
    Scratch sc = arena_scratch_push(a);
    Iter it = avl_iter(t);
    int prev, cur;
    cr_assert(it.next(&it, &prev));
    while (it.next(&it, &cur))
    {
        cr_assert_lt(prev, cur);
        prev = cur;
    }
    iter_drop(&it);
    arena_scratch_pop(&sc);
    arena_free(a);
}

Test(avl, remove_rebalances)
{
    /* Insert ascending 1..7, remove the root repeatedly and verify balance */
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    for (int i = 1; i <= 7; i++)
        avl_insert(t, &i);
    for (int i = 1; i <= 6; i++)
    {
        cr_assert_eq(avl_remove(t, &i), SEQC_OK);
        cr_assert_eq(avl_len(t), (size_t)(7 - i));
    }
    cr_assert_eq(avl_len(t), 1);
    arena_free(a);
}

Test(avl, remove_nonexistent_returns_0)
{
    Arena *a = arena_create(256);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int v = 42;
    cr_assert_neq(avl_remove(t, &v), SEQC_OK);
    arena_free(a);
}

/* ---- balance invariant ------------------------------------------------- */

Test(avl, height_stays_logarithmic)
{
    /* Insert 1000 ascending integers — worst case for an unbalanced BST
     * (would be height 1000); AVL must keep it at ~log2(1000) ≈ 10. */
    Arena *a = arena_create(65536);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    for (int i = 0; i < 1000; i++)
        avl_insert(t, &i);
    cr_assert_eq(avl_len(t), 1000);
    /* AVL height bound: <= 1.44 * log2(n+2) - 0.328 */
    int max_height = 2 * log2_ceil(1002);
    cr_assert_leq(avl_height(t), max_height);
    arena_free(a);
}

Test(avl, many_inserts_sorted_output)
{
    Arena *a = arena_create(8192);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int order[] = {24, 12, 36, 6,  18, 30, 42, 3,  9,  15, 21, 27, 33,
                   39, 45, 1,  4,  7,  10, 13, 16, 19, 22, 25, 28, 31,
                   34, 37, 40, 43, 46, 0,  2,  5,  8,  11, 14, 17, 20,
                   23, 26, 29, 32, 35, 38, 41, 44, 47, 48, 49};
    for (int i = 0; i < 50; i++)
        avl_insert(t, &order[i]);
    cr_assert_eq(avl_len(t), 50);
    cr_assert_eq(*(int *)avl_min(t), 0);
    cr_assert_eq(*(int *)avl_max(t), 49);
    Scratch sc = arena_scratch_push(a);
    Iter it = avl_iter(t);
    int prev, cur;
    cr_assert(it.next(&it, &prev));
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
    arena_scratch_pop(&sc);
    arena_free(a);
}

Test(avl, iter_rev_descending)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {4, 2, 6, 1, 3, 5, 7};
    for (int i = 0; i < 7; i++)
        avl_insert(t, &vals[i]);
    Scratch sc = arena_scratch_push(a);
    Iter it = avl_iter_rev(t);
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
    arena_scratch_pop(&sc);
    arena_free(a);
}

Test(avl, iter_range_mid)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
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
    cr_assert_eq(n, 5); /* 3,4,5,6,7 */
    cr_assert_eq(collected[0], 3);
    cr_assert_eq(collected[4], 7);
    arena_free(a);
}

Test(avl, iter_range_no_lo)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
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
    cr_assert_eq(n, 3); /* 1,2,3 */
    arena_free(a);
}

Test(avl, iter_range_no_hi)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
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
    cr_assert_eq(n, 3); /* 3,4,5 */
    arena_free(a);
}

Test(avl, iter_range_empty_result)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {1, 5, 10};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    int lo = 6, hi = 9;
    Iter it = avl_iter_range(t, &lo, &hi);
    int v;
    cr_assert(!it.next(&it, &v));
    iter_drop(&it);
    arena_free(a);
}

/* ---- avl_clear --------------------------------------------------------- */

Test(avl, clear_empties_tree)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {3, 1, 5, 2, 4};
    for (int i = 0; i < 5; i++)
        avl_insert(t, &vals[i]);
    avl_clear(t);
    cr_assert_eq(avl_len(t), 0);
    cr_assert_null(avl_min(t));
    cr_assert_null(avl_max(t));
    arena_free(a);
}

Test(avl, clear_allows_reuse)
{
    Arena *a = arena_create(1024);
    AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
    int vals[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
        avl_insert(t, &vals[i]);
    avl_clear(t);
    int x = 42;
    cr_assert_eq(avl_insert(t, &x), SEQC_OK);
    cr_assert_eq(avl_len(t), 1);
    cr_assert(avl_contains(t, &x));
    arena_free(a);
}
