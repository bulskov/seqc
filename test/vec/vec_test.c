#include <criterion/criterion.h>

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/vec.h"
#include "../oom_alloc.h"

Test(vec, create_is_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    cr_assert_eq(vec_len(v), 0);

    vec_free(v);
    growing_arena_destroy(a);
}

Test(vec, push_increments_len)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    cr_assert_eq(vec_len(v), 1);
    vec_free(v);
    growing_arena_destroy(a);
}

Test(vec, get_returns_pushed_value)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 99;
    vec_push(v, &x);
    cr_assert_eq(*(int *)vec_get(v, 0), 99);
    vec_free(v);
    growing_arena_destroy(a);
}

Test(vec, push_many_preserves_values)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 100; i++)
        vec_push(v, &i);
    cr_assert_eq(vec_len(v), 100);
    for (int i = 0; i < 100; i++)
        cr_assert_eq(*(int *)vec_get(v, (size_t)i), i);
    vec_free(v);
    growing_arena_destroy(a);
}

Test(vec, as_slice_reflects_contents)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 7, y = 8, z = 9;
    vec_push(v, &x);
    vec_push(v, &y);
    vec_push(v, &z);

    Slice s = vec_as_slice(v);
    cr_assert_eq(s.len, 3);
    cr_assert_eq(*(int *)slice_get(s, 1), 8);
    vec_free(v);
    growing_arena_destroy(a);
}

Test(vec, iter_counts_all_elements)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        vec_push(v, &i);

    cr_assert_eq(iter_count(vec_iter(v)), 5);
    vec_free(v);
    growing_arena_destroy(a);
}

Test(vec, iter_collect_round_trip)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        vec_push(v, &i);

    Slice result = iter_collect(vec_iter(v), growing_arena_allocator(a));

    cr_assert_eq(result.len, 4);
    for (int i = 0; i < 4; i++)
        cr_assert_eq(*(int *)slice_get(result, (size_t)i), i);

    vec_free(v);
    growing_arena_destroy(a);
}

Test(vec, iter_rev)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        vec_push(v, &i);
    Iter it = vec_iter_rev(v);
    int val;
    for (int expected = 4; expected >= 0; expected--)
    {
        cr_assert(it.next(&it, &val));
        cr_assert_eq(val, expected);
    }
    cr_assert_not(it.next(&it, &val));
    iter_drop(&it);
    growing_arena_destroy(a);
}

/* ---- vec_pop ----------------------------------------------------------- */

Test(vec, pop_returns_last_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    int out;
    cr_assert_eq(vec_pop(v, &out), SEQC_OK);
    cr_assert_eq(out, 2);
    cr_assert_eq(vec_len(v), 2);
    growing_arena_destroy(a);
}

Test(vec, pop_empty_returns_false)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    cr_assert_neq(vec_pop(v, NULL), SEQC_OK);
    growing_arena_destroy(a);
}

Test(vec, pop_discard_with_null_out)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 7;
    vec_push(v, &x);
    cr_assert_eq(vec_pop(v, NULL), SEQC_OK);
    cr_assert_eq(vec_len(v), 0);
    growing_arena_destroy(a);
}

/* ---- vec_set ----------------------------------------------------------- */

Test(vec, set_overwrites_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    int val = 99;
    vec_set(v, 1, &val);
    cr_assert_eq(*(int *)vec_get(v, 0), 0);
    cr_assert_eq(*(int *)vec_get(v, 1), 99);
    cr_assert_eq(*(int *)vec_get(v, 2), 2);
    growing_arena_destroy(a);
}

/* ---- vec_reserve ------------------------------------------------------- */

Test(vec, reserve_grows_capacity)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    vec_reserve(v, 64);
    cr_assert_geq(vec_cap(v), 64);
    growing_arena_destroy(a);
}

Test(vec, reserve_does_not_shrink)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    vec_reserve(v, 64);
    size_t cap = vec_cap(v);
    vec_reserve(v, 4);
    cr_assert_eq(vec_cap(v), cap);
    growing_arena_destroy(a);
}

/* ---- vec_insert -------------------------------------------------------- */

Test(vec, insert_at_beginning)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 1; i <= 3; i++)
        vec_push(v, &i);
    int val = 0;
    vec_insert(v, 0, &val);
    cr_assert_eq(vec_len(v), 4);
    cr_assert_eq(*(int *)vec_get(v, 0), 0);
    cr_assert_eq(*(int *)vec_get(v, 1), 1);
    cr_assert_eq(*(int *)vec_get(v, 3), 3);
    growing_arena_destroy(a);
}

Test(vec, insert_in_middle)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 3};
    vec_push(v, &vals[0]);
    vec_push(v, &vals[1]);
    int mid = 2;
    vec_insert(v, 1, &mid);
    cr_assert_eq(vec_len(v), 3);
    cr_assert_eq(*(int *)vec_get(v, 0), 1);
    cr_assert_eq(*(int *)vec_get(v, 1), 2);
    cr_assert_eq(*(int *)vec_get(v, 2), 3);
    growing_arena_destroy(a);
}

Test(vec, insert_at_end_equals_push)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    int val = 99;
    vec_insert(v, vec_len(v), &val);
    cr_assert_eq(vec_len(v), 4);
    cr_assert_eq(*(int *)vec_get(v, 3), 99);
    growing_arena_destroy(a);
}

/* ---- vec_remove -------------------------------------------------------- */

Test(vec, remove_first_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    vec_remove(v, 0);
    cr_assert_eq(vec_len(v), 2);
    cr_assert_eq(*(int *)vec_get(v, 0), 1);
    cr_assert_eq(*(int *)vec_get(v, 1), 2);
    growing_arena_destroy(a);
}

Test(vec, remove_middle_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        vec_push(v, &i);
    vec_remove(v, 2);
    cr_assert_eq(vec_len(v), 3);
    cr_assert_eq(*(int *)vec_get(v, 0), 0);
    cr_assert_eq(*(int *)vec_get(v, 1), 1);
    cr_assert_eq(*(int *)vec_get(v, 2), 3);
    growing_arena_destroy(a);
}

Test(vec, remove_last_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    vec_remove(v, 2);
    cr_assert_eq(vec_len(v), 2);
    cr_assert_eq(*(int *)vec_get(v, 1), 1);
    growing_arena_destroy(a);
}

/* ---- vec_clear --------------------------------------------------------- */

Test(vec, clear_resets_len)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        vec_push(v, &i);
    size_t cap = vec_cap(v);
    vec_clear(v);
    cr_assert_eq(vec_len(v), 0);
    cr_assert_eq(vec_cap(v), cap); /* buffer retained */
    growing_arena_destroy(a);
}

Test(vec, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    vec_clear(v);
    int x = 42;
    vec_push(v, &x);
    cr_assert_eq(vec_len(v), 1);
    cr_assert_eq(*(int *)vec_get(v, 0), 42);
    growing_arena_destroy(a);
}

/* ---- vec_find / vec_contains ------------------------------------------- */

static bool int_gt_three(const void *elem, void *ctx)
{
    (void)ctx;
    return *(const int *)elem > 3;
}

Test(vec, find_returns_first_match)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 4, 3, 5};
    for (int i = 0; i < 5; i++)
        vec_push(v, &vals[i]);
    int *p = vec_find(v, int_gt_three, NULL);
    cr_assert_not_null(p);
    cr_assert_eq(*p, 4); /* first element > 3 */
    growing_arena_destroy(a);
}

Test(vec, find_returns_null_when_no_match)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        vec_push(v, &vals[i]);
    cr_assert_null(vec_find(v, int_gt_three, NULL));
    growing_arena_destroy(a);
}

Test(vec, contains_returns_true_when_match_exists)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 5};
    for (int i = 0; i < 3; i++)
        vec_push(v, &vals[i]);
    cr_assert(vec_contains(v, int_gt_three, NULL));
    growing_arena_destroy(a);
}

Test(vec, contains_returns_false_when_no_match)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        vec_push(v, &vals[i]);
    cr_assert(!vec_contains(v, int_gt_three, NULL));
    growing_arena_destroy(a);
}

/* vec_get with an out-of-bounds index must return NULL. */
Test(vec, get_out_of_bounds_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    cr_assert_null(vec_get(v, 1)); /* only index 0 is valid */
    cr_assert_null(vec_get(v, 99));
    growing_arena_destroy(a);
}

/* vec_insert into a full vec must trigger an internal grow. */
Test(vec, insert_when_full_triggers_grow)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    /* Fill exactly to capacity (INITIAL_CAP = 16). */
    for (int i = 0; i < 16; i++)
        vec_push(v, &i);
    cr_assert_eq(vec_len(v), vec_cap(v)); /* at capacity before insert */
    int newval = 99;
    vec_insert(v, 0, &newval); /* insert at front triggers grow */
    cr_assert_eq(*(int *)vec_get(v, 0), 99);
    cr_assert_eq(*(int *)vec_get(v, 1), 0);
    cr_assert_eq(vec_len(v), 17);
    growing_arena_destroy(a);
}

/* ---- OOM paths --------------------------------------------------------- */

Test(vec, create_returns_null_on_oom)
{
    Vec *v = vec_create(sizeof(int), null_allocator());
    cr_assert_null(v);
}

Test(vec, push_returns_oom_when_grow_fails)
{
    /* alloc #1 succeeds (Vec struct), alloc #2 (data buffer grow) fails */
    OomCtx ctx;
    allocator_t al = oom_after_allocator(1, &ctx);
    Vec *v = vec_create(sizeof(int), al);
    cr_assert_not_null(v);
    int x = 1;
    cr_assert_eq(vec_push(v, &x), SEQC_OOM);
    cr_assert_eq(vec_len(v), 0);
    free(v); /* allocated by oom_alloc's malloc */
}

/* ---- vec_sort ---------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

Test(vec, sort_orders_elements)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {5, 2, 8, 1, 9, 3};
    for (int i = 0; i < 6; i++)
        vec_push(v, &vals[i]);
    vec_sort(v, int_cmp);
    for (size_t i = 1; i < vec_len(v); i++)
        cr_assert_leq(*(int *)vec_get(v, i - 1), *(int *)vec_get(v, i));
    growing_arena_destroy(a);
}

Test(vec, sort_empty_is_noop)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    vec_sort(v, int_cmp); /* must not crash */
    cr_assert_eq(vec_len(v), 0);
    growing_arena_destroy(a);
}

Test(vec, sort_single_element_is_noop)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    vec_sort(v, int_cmp);
    cr_assert_eq(*(int *)vec_get(v, 0), 42);
    growing_arena_destroy(a);
}
