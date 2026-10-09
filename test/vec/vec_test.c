#include "ctt.h"

#include "arena/growing_arena.h"
#include "seqc/vec.h"

#include "../oom_alloc.h"

TEST(vec_create_is_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_EQ(0, vec_len(v));

    vec_destroy(v);
    growing_arena_destroy(a);
}

TEST(vec_push_increments_len)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    ASSERT_EQ(1, vec_len(v));
    vec_destroy(v);
    growing_arena_destroy(a);
}

TEST(vec_get_returns_pushed_value)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 99;
    vec_push(v, &x);
    ASSERT_EQ(99, *(int *)vec_get_ptr(v, 0));
    vec_destroy(v);
    growing_arena_destroy(a);
}

TEST(vec_push_many_preserves_values)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 100; i++)
    {
        vec_push(v, &i);
    }
    ASSERT_EQ(100, vec_len(v));
    for (int i = 0; i < 100; i++)
    {
        ASSERT_EQ(i, *(int *)vec_get_ptr(v, (size_t)i));
    }
    vec_destroy(v);
    growing_arena_destroy(a);
}

TEST(vec_as_slice_reflects_contents)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 7, y = 8, z = 9;
    vec_push(v, &x);
    vec_push(v, &y);
    vec_push(v, &z);

    slice_t s = vec_as_slice(v);
    ASSERT_EQ(3, s.len);
    ASSERT_EQ(8, *(int *)slice_get_ptr(s, 1));
    vec_destroy(v);
    growing_arena_destroy(a);
}

TEST(vec_iter_counts_all_elements)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        vec_push(v, &i);
    }

    ASSERT_EQ(5, iter_count(vec_iter(v)));
    vec_destroy(v);
    growing_arena_destroy(a);
}

TEST(vec_iter_collect_round_trip)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
    {
        vec_push(v, &i);
    }

    slice_t result = iter_collect(vec_iter(v), growing_arena_allocator(a));

    ASSERT_EQ(4, result.len);
    for (int i = 0; i < 4; i++)
    {
        ASSERT_EQ(i, *(int *)slice_get_ptr(result, (size_t)i));
    }

    vec_destroy(v);
    growing_arena_destroy(a);
}

TEST(vec_iter_rev_yields_reverse_order)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        vec_push(v, &i);
    }
    iter_t it = vec_iter_rev(v);
    int val;
    for (int expected = 4; expected >= 0; expected--)
    {
        ASSERT_TRUE(it.next(&it, &val));
        ASSERT_EQ(expected, val);
    }
    ASSERT_FALSE(it.next(&it, &val));
    iter_destroy(&it);
    growing_arena_destroy(a);
}

/* ---- vec_pop ----------------------------------------------------------- */

TEST(vec_pop_returns_last_element)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &i);
    }
    int out;
    ASSERT_EQ(SEQC_OK, vec_pop(v, &out));
    ASSERT_EQ(2, out);
    ASSERT_EQ(2, vec_len(v));
    growing_arena_destroy(a);
}

TEST(vec_pop_empty_returns_false)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_NE(SEQC_OK, vec_pop(v, NULL));
    growing_arena_destroy(a);
}

TEST(vec_pop_discard_with_null_out)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 7;
    vec_push(v, &x);
    ASSERT_EQ(SEQC_OK, vec_pop(v, NULL));
    ASSERT_EQ(0, vec_len(v));
    growing_arena_destroy(a);
}

/* ---- vec_set ----------------------------------------------------------- */

TEST(vec_set_overwrites_element)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &i);
    }
    int val = 99;
    vec_set(v, 1, &val);
    ASSERT_EQ(0, *(int *)vec_get_ptr(v, 0));
    ASSERT_EQ(99, *(int *)vec_get_ptr(v, 1));
    ASSERT_EQ(2, *(int *)vec_get_ptr(v, 2));
    growing_arena_destroy(a);
}

/* ---- vec_reserve ------------------------------------------------------- */

TEST(vec_reserve_grows_capacity)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    vec_reserve(v, 64);
    ASSERT_GE(vec_cap(v), 64);
    growing_arena_destroy(a);
}

TEST(vec_reserve_does_not_shrink)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    vec_reserve(v, 64);
    size_t cap = vec_cap(v);
    vec_reserve(v, 4);
    ASSERT_EQ(cap, vec_cap(v));
    growing_arena_destroy(a);
}

/* ---- vec_insert -------------------------------------------------------- */

TEST(vec_insert_at_beginning)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 1; i <= 3; i++)
    {
        vec_push(v, &i);
    }
    int val = 0;
    vec_insert(v, 0, &val);
    ASSERT_EQ(4, vec_len(v));
    ASSERT_EQ(0, *(int *)vec_get_ptr(v, 0));
    ASSERT_EQ(1, *(int *)vec_get_ptr(v, 1));
    ASSERT_EQ(3, *(int *)vec_get_ptr(v, 3));
    growing_arena_destroy(a);
}

TEST(vec_insert_in_middle)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 3};
    vec_push(v, &vals[0]);
    vec_push(v, &vals[1]);
    int mid = 2;
    vec_insert(v, 1, &mid);
    ASSERT_EQ(3, vec_len(v));
    ASSERT_EQ(1, *(int *)vec_get_ptr(v, 0));
    ASSERT_EQ(2, *(int *)vec_get_ptr(v, 1));
    ASSERT_EQ(3, *(int *)vec_get_ptr(v, 2));
    growing_arena_destroy(a);
}

TEST(vec_insert_at_end_equals_push)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &i);
    }
    int val = 99;
    vec_insert(v, vec_len(v), &val);
    ASSERT_EQ(4, vec_len(v));
    ASSERT_EQ(99, *(int *)vec_get_ptr(v, 3));
    growing_arena_destroy(a);
}

/* ---- vec_remove -------------------------------------------------------- */

TEST(vec_remove_first_element)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &i);
    }
    vec_remove(v, 0);
    ASSERT_EQ(2, vec_len(v));
    ASSERT_EQ(1, *(int *)vec_get_ptr(v, 0));
    ASSERT_EQ(2, *(int *)vec_get_ptr(v, 1));
    growing_arena_destroy(a);
}

TEST(vec_remove_middle_element)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
    {
        vec_push(v, &i);
    }
    vec_remove(v, 2);
    ASSERT_EQ(3, vec_len(v));
    ASSERT_EQ(0, *(int *)vec_get_ptr(v, 0));
    ASSERT_EQ(1, *(int *)vec_get_ptr(v, 1));
    ASSERT_EQ(3, *(int *)vec_get_ptr(v, 2));
    growing_arena_destroy(a);
}

TEST(vec_remove_last_element)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &i);
    }
    vec_remove(v, 2);
    ASSERT_EQ(2, vec_len(v));
    ASSERT_EQ(1, *(int *)vec_get_ptr(v, 1));
    growing_arena_destroy(a);
}

/* ---- vec_clear --------------------------------------------------------- */

TEST(vec_clear_resets_len)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        vec_push(v, &i);
    }
    size_t cap = vec_cap(v);
    vec_clear(v);
    ASSERT_EQ(0, vec_len(v));
    ASSERT_EQ(cap, vec_cap(v)); /* buffer retained */
    growing_arena_destroy(a);
}

TEST(vec_clear_allows_reuse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &i);
    }
    vec_clear(v);
    int x = 42;
    vec_push(v, &x);
    ASSERT_EQ(1, vec_len(v));
    ASSERT_EQ(42, *(int *)vec_get_ptr(v, 0));
    growing_arena_destroy(a);
}

/* ---- vec_find / vec_any ------------------------------------------- */

static bool int_gt_three(const void *elem, void *ctx)
{
    (void)ctx;
    return *(const int *)elem > 3;
}

TEST(vec_find_returns_first_match)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 4, 3, 5};
    for (int i = 0; i < 5; i++)
    {
        vec_push(v, &vals[i]);
    }
    int *p = (int *)vec_find(v, int_gt_three, NULL);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(4, *p); /* first element > 3 */
    growing_arena_destroy(a);
}

TEST(vec_find_returns_null_when_no_match)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &vals[i]);
    }
    ASSERT_NULL(vec_find(v, int_gt_three, NULL));
    growing_arena_destroy(a);
}

TEST(vec_contains_returns_true_when_match_exists)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 5};
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &vals[i]);
    }
    ASSERT_TRUE(vec_any(v, int_gt_three, NULL));
    growing_arena_destroy(a);
}

TEST(vec_contains_returns_false_when_no_match)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &vals[i]);
    }
    ASSERT_TRUE(!vec_any(v, int_gt_three, NULL));
    growing_arena_destroy(a);
}

/* vec_get_ptr with an out-of-bounds index must return NULL. */
TEST(vec_get_out_of_bounds_returns_null)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    ASSERT_NULL(vec_get_ptr(v, 1)); /* only index 0 is valid */
    ASSERT_NULL(vec_get_ptr(v, 99));
    growing_arena_destroy(a);
}

/* vec_insert into a full vec must trigger an internal grow. */
TEST(vec_insert_when_full_triggers_grow)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    /* Fill exactly to capacity (INITIAL_CAP = 16). */
    for (int i = 0; i < 16; i++)
    {
        vec_push(v, &i);
    }
    ASSERT_EQ(vec_cap(v), vec_len(v)); /* at capacity before insert */
    int newval = 99;
    vec_insert(v, 0, &newval); /* insert at front triggers grow */
    ASSERT_EQ(99, *(int *)vec_get_ptr(v, 0));
    ASSERT_EQ(0, *(int *)vec_get_ptr(v, 1));
    ASSERT_EQ(17, vec_len(v));
    growing_arena_destroy(a);
}

/* ---- OOM paths --------------------------------------------------------- */

TEST(vec_create_returns_null_on_oom)
{
    vec_t *v = vec_create(sizeof(int), null_allocator());
    ASSERT_NULL(v);
}

TEST(vec_push_returns_oom_when_grow_fails)
{
    /* alloc #1 succeeds (vec_t struct), alloc #2 (data buffer grow) fails */
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(1, &ctx);
    vec_t *v = vec_create(sizeof(int), al);
    ASSERT_NOT_NULL(v);
    int x = 1;
    ASSERT_EQ(SEQC_OOM, vec_push(v, &x));
    ASSERT_EQ(0, vec_len(v));
    free(v); /* allocated by oom_alloc's malloc */
}

/* ---- vec_sort ---------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

TEST(vec_sort_orders_elements)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {5, 2, 8, 1, 9, 3};
    for (int i = 0; i < 6; i++)
    {
        vec_push(v, &vals[i]);
    }
    vec_sort(v, int_cmp);
    for (size_t i = 1; i < vec_len(v); i++)
    {
        ASSERT_LE(*(int *)vec_get_ptr(v, i - 1), *(int *)vec_get_ptr(v, i));
    }
    growing_arena_destroy(a);
}

TEST(vec_sort_empty_is_noop)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    vec_sort(v, int_cmp); /* must not crash */
    ASSERT_EQ(0, vec_len(v));
    growing_arena_destroy(a);
}

TEST(vec_sort_single_element_is_noop)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    vec_sort(v, int_cmp);
    ASSERT_EQ(42, *(int *)vec_get_ptr(v, 0));
    growing_arena_destroy(a);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "vec_test");
}
