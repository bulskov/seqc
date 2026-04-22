#include <criterion/criterion.h>

#include "arena/arena.h"
#include "vec/vec.h"

Test(vec, create_is_empty)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    cr_assert_eq(vec_len(v), 0);
    
    vec_free(v);
    arena_free(a);
}

Test(vec, push_increments_len)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    cr_assert_eq(vec_len(v), 1);
    vec_free(v);
    arena_free(a);
}

Test(vec, get_returns_pushed_value)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int x = 99;
    vec_push(v, &x);
    cr_assert_eq(*(int *)vec_get(v, 0), 99);
    vec_free(v);
    arena_free(a);
}

Test(vec, push_many_preserves_values)
{
    Arena *a = arena_create(4096);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 100; i++)
        vec_push(v, &i);
    cr_assert_eq(vec_len(v), 100);
    for (int i = 0; i < 100; i++)
        cr_assert_eq(*(int *)vec_get(v, (size_t)i), i);
    vec_free(v);
    arena_free(a);
}

Test(vec, as_slice_reflects_contents)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int x = 7, y = 8, z = 9;
    vec_push(v, &x);
    vec_push(v, &y);
    vec_push(v, &z);

    Slice s = vec_as_slice(v);
    cr_assert_eq(s.len, 3);
    cr_assert_eq(*(int *)slice_get(s, 1), 8);
    vec_free(v);
    arena_free(a);
}

Test(vec, iter_counts_all_elements)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 5; i++)
        vec_push(v, &i);

    cr_assert_eq(iter_count(vec_iter(v)), 5);
    vec_free(v);
    arena_free(a);
}

Test(vec, iter_collect_round_trip)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 4; i++)
        vec_push(v, &i);

    Slice result = iter_collect(vec_iter(v), arena_allocator(a));

    cr_assert_eq(result.len, 4);
    for (int i = 0; i < 4; i++)
        cr_assert_eq(*(int *)slice_get(result, (size_t)i), i);

    vec_free(v);
    arena_free(a);
}

Test(vec, iter_rev)
{
    Arena *a = arena_create(512);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
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
    arena_free(a);
}

/* ---- vec_pop ----------------------------------------------------------- */

Test(vec, pop_returns_last_element)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    int out;
    cr_assert(vec_pop(v, &out));
    cr_assert_eq(out, 2);
    cr_assert_eq(vec_len(v), 2);
    arena_free(a);
}

Test(vec, pop_empty_returns_false)
{
    Arena *a = arena_create(64);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    cr_assert_not(vec_pop(v, NULL));
    arena_free(a);
}

Test(vec, pop_discard_with_null_out)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int x = 7;
    vec_push(v, &x);
    cr_assert(vec_pop(v, NULL));
    cr_assert_eq(vec_len(v), 0);
    arena_free(a);
}

/* ---- vec_set ----------------------------------------------------------- */

Test(vec, set_overwrites_element)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    int val = 99;
    vec_set(v, 1, &val);
    cr_assert_eq(*(int *)vec_get(v, 0), 0);
    cr_assert_eq(*(int *)vec_get(v, 1), 99);
    cr_assert_eq(*(int *)vec_get(v, 2), 2);
    arena_free(a);
}

/* ---- vec_reserve ------------------------------------------------------- */

Test(vec, reserve_grows_capacity)
{
    Arena *a = arena_create(1024);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    vec_reserve(v, 64);
    cr_assert_geq(vec_cap(v), 64);
    arena_free(a);
}

Test(vec, reserve_does_not_shrink)
{
    Arena *a = arena_create(1024);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    vec_reserve(v, 64);
    size_t cap = vec_cap(v);
    vec_reserve(v, 4);
    cr_assert_eq(vec_cap(v), cap);
    arena_free(a);
}

/* ---- vec_insert -------------------------------------------------------- */

Test(vec, insert_at_beginning)
{
    Arena *a = arena_create(512);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 1; i <= 3; i++)
        vec_push(v, &i);
    int val = 0;
    vec_insert(v, 0, &val);
    cr_assert_eq(vec_len(v), 4);
    cr_assert_eq(*(int *)vec_get(v, 0), 0);
    cr_assert_eq(*(int *)vec_get(v, 1), 1);
    cr_assert_eq(*(int *)vec_get(v, 3), 3);
    arena_free(a);
}

Test(vec, insert_in_middle)
{
    Arena *a = arena_create(512);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int vals[] = {1, 3};
    vec_push(v, &vals[0]);
    vec_push(v, &vals[1]);
    int mid = 2;
    vec_insert(v, 1, &mid);
    cr_assert_eq(vec_len(v), 3);
    cr_assert_eq(*(int *)vec_get(v, 0), 1);
    cr_assert_eq(*(int *)vec_get(v, 1), 2);
    cr_assert_eq(*(int *)vec_get(v, 2), 3);
    arena_free(a);
}

Test(vec, insert_at_end_equals_push)
{
    Arena *a = arena_create(512);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    int val = 99;
    vec_insert(v, vec_len(v), &val);
    cr_assert_eq(vec_len(v), 4);
    cr_assert_eq(*(int *)vec_get(v, 3), 99);
    arena_free(a);
}

/* ---- vec_remove -------------------------------------------------------- */

Test(vec, remove_first_element)
{
    Arena *a = arena_create(512);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    vec_remove(v, 0);
    cr_assert_eq(vec_len(v), 2);
    cr_assert_eq(*(int *)vec_get(v, 0), 1);
    cr_assert_eq(*(int *)vec_get(v, 1), 2);
    arena_free(a);
}

Test(vec, remove_middle_element)
{
    Arena *a = arena_create(512);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 4; i++)
        vec_push(v, &i);
    vec_remove(v, 2);
    cr_assert_eq(vec_len(v), 3);
    cr_assert_eq(*(int *)vec_get(v, 0), 0);
    cr_assert_eq(*(int *)vec_get(v, 1), 1);
    cr_assert_eq(*(int *)vec_get(v, 2), 3);
    arena_free(a);
}

Test(vec, remove_last_element)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    vec_remove(v, 2);
    cr_assert_eq(vec_len(v), 2);
    cr_assert_eq(*(int *)vec_get(v, 1), 1);
    arena_free(a);
}

/* ---- vec_clear --------------------------------------------------------- */

Test(vec, clear_resets_len)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 5; i++)
        vec_push(v, &i);
    size_t cap = vec_cap(v);
    vec_clear(v);
    cr_assert_eq(vec_len(v), 0);
    cr_assert_eq(vec_cap(v), cap); /* buffer retained */
    arena_free(a);
}

Test(vec, clear_allows_reuse)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 3; i++)
        vec_push(v, &i);
    vec_clear(v);
    int x = 42;
    vec_push(v, &x);
    cr_assert_eq(vec_len(v), 1);
    cr_assert_eq(*(int *)vec_get(v, 0), 42);
    arena_free(a);
}

/* ---- vec_find / vec_contains ------------------------------------------- */

static bool int_gt_three(const void *elem, void *ctx)
{
    (void)ctx;
    return *(const int *)elem > 3;
}

Test(vec, find_returns_first_match)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int vals[] = {1, 2, 4, 3, 5};
    for (int i = 0; i < 5; i++)
        vec_push(v, &vals[i]);
    int *p = vec_find(v, int_gt_three, NULL);
    cr_assert_not_null(p);
    cr_assert_eq(*p, 4); /* first element > 3 */
    arena_free(a);
}

Test(vec, find_returns_null_when_no_match)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        vec_push(v, &vals[i]);
    cr_assert_null(vec_find(v, int_gt_three, NULL));
    arena_free(a);
}

Test(vec, contains_returns_true_when_match_exists)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int vals[] = {1, 2, 5};
    for (int i = 0; i < 3; i++)
        vec_push(v, &vals[i]);
    cr_assert(vec_contains(v, int_gt_three, NULL));
    arena_free(a);
}

Test(vec, contains_returns_false_when_no_match)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        vec_push(v, &vals[i]);
    cr_assert(!vec_contains(v, int_gt_three, NULL));
    arena_free(a);
}

/* vec_get with an out-of-bounds index must return NULL. */
Test(vec, get_out_of_bounds_returns_null)
{
    Arena *a = arena_create(256);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    cr_assert_null(vec_get(v, 1)); /* only index 0 is valid */
    cr_assert_null(vec_get(v, 99));
    arena_free(a);
}

/* vec_insert into a full vec must trigger an internal grow. */
Test(vec, insert_when_full_triggers_grow)
{
    Arena *a = arena_create(4096);
    Vec *v = vec_create(sizeof(int), arena_allocator(a));
    /* Fill exactly to capacity (INITIAL_CAP = 16). */
    for (int i = 0; i < 16; i++)
        vec_push(v, &i);
    cr_assert_eq(vec_len(v), vec_cap(v)); /* at capacity before insert */
    int newval = 99;
    vec_insert(v, 0, &newval); /* insert at front triggers grow */
    cr_assert_eq(*(int *)vec_get(v, 0), 99);
    cr_assert_eq(*(int *)vec_get(v, 1), 0);
    cr_assert_eq(vec_len(v), 17);
    arena_free(a);
}
