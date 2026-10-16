#include "ctt.h"

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/iter.h"
#include "seqc/string.h"
#include "seqc/vec.h"

#include <stdlib.h>

/* ---- helpers ----------------------------------------------------------- */

static bool gt10(const void *elem, void *ctx)
{
    (void)ctx;
    return *(const int *)elem > 10;
}

static void double_it(const void *in, void *out, void *ctx)
{
    (void)ctx;
    *(int *)out = *(const int *)in * 2;
}

static void sum_combine(void *acc, const void *elem, void *ctx)
{
    (void)ctx;
    *(int *)acc += *(const int *)elem;
}

static void push_to_arr(const void *elem, void *ctx)
{
    int **p = (int **)ctx;
    *(*p)++ = *(const int *)elem;
}

/* ---- tests ------------------------------------------------------------- */

TEST(iter_from_slice_count)
{
    int data[] = {1, 2, 3, 4, 5};
    slice_t s = {data, 5, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    ASSERT_EQ(5, iter_count(iter_from_slice(s, scratch_allocator(&sc))));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_from_slice_empty)
{
    slice_t s = {NULL, 0, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    ASSERT_EQ(0, iter_count(iter_from_slice(s, scratch_allocator(&sc))));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_filter_keeps_matching)
{
    int data[] = {3, 15, 7, 22, 1, 18};
    slice_t s = {data, 6, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(
        iter_filter(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    ASSERT_EQ(3, n);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_filter_none_match)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(
        iter_filter(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    ASSERT_EQ(0, n);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_map_doubles_values)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);

    slice_t result = iter_collect(
        iter_map(
            iter_from_slice(s, growing_arena_allocator(a)),
            double_it,
            NULL,
            sizeof(int)),
        growing_arena_allocator(a));

    ASSERT_EQ(3, result.len);
    ASSERT_EQ(2, *(int *)slice_get_ptr(result, 0));
    ASSERT_EQ(4, *(int *)slice_get_ptr(result, 1));
    ASSERT_EQ(6, *(int *)slice_get_ptr(result, 2));

    growing_arena_destroy(a);
}

TEST(iter_collect_produces_correct_slice)
{
    int data[] = {10, 20, 30, 40};
    slice_t s = {data, 4, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);

    slice_t result = iter_collect(
        iter_from_slice(s, growing_arena_allocator(a)),
        growing_arena_allocator(a));

    ASSERT_EQ(4, result.len);
    for (size_t i = 0; i < result.len; i++)
    {
        ASSERT_EQ(data[i], *(int *)slice_get_ptr(result, i));
    }

    growing_arena_destroy(a);
}

TEST(iter_collect_empty_gives_null_ptr)
{
    slice_t s = {NULL, 0, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);

    slice_t result = iter_collect(
        iter_from_slice(s, growing_arena_allocator(a)),
        growing_arena_allocator(a));

    ASSERT_EQ(0, result.len);
    ASSERT_NULL(result.ptr);

    growing_arena_destroy(a);
}

TEST(iter_take_limits_output)
{
    int data[] = {1, 2, 3, 4, 5};
    slice_t s = {data, 5, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    ASSERT_EQ(
        3,
        iter_count(iter_take(iter_from_slice(s, scratch_allocator(&sc)), 3)));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_take_more_than_available)
{
    int data[] = {1, 2};
    slice_t s = {data, 2, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    ASSERT_EQ(
        2,
        iter_count(iter_take(iter_from_slice(s, scratch_allocator(&sc)), 100)));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_skip_drops_first_n)
{
    int data[] = {1, 2, 3, 4, 5};
    slice_t s = {data, 5, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    ASSERT_EQ(
        2,
        iter_count(iter_skip(iter_from_slice(s, scratch_allocator(&sc)), 3)));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_skip_all)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    ASSERT_EQ(
        0,
        iter_count(iter_skip(iter_from_slice(s, scratch_allocator(&sc)), 10)));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_reduce_sum)
{
    int data[] = {1, 2, 3, 4, 5};
    slice_t s = {data, 5, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    int sum = 0;
    iter_reduce(
        iter_from_slice(s, scratch_allocator(&sc)), &sum, sum_combine, NULL);
    ASSERT_EQ(15, sum);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_foreach_visits_all)
{
    int data[] = {10, 20, 30};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    int out[3] = {0};
    int *ptr = out;
    iter_foreach(iter_from_slice(s, scratch_allocator(&sc)), push_to_arr, &ptr);
    ASSERT_EQ(10, out[0]);
    ASSERT_EQ(20, out[1]);
    ASSERT_EQ(30, out[2]);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_filter_map_chain)
{
    int data[] = {3, 15, 7, 22, 1, 18};
    slice_t s = {data, 6, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);

    slice_t result = iter_collect(
        iter_map(
            iter_filter(
                iter_from_slice(s, growing_arena_allocator(a)), gt10, NULL),
            double_it,
            NULL,
            sizeof(int)),
        growing_arena_allocator(a));

    /* 15→30, 22→44, 18→36 */
    ASSERT_EQ(3, result.len);
    ASSERT_EQ(30, *(int *)slice_get_ptr(result, 0));
    ASSERT_EQ(44, *(int *)slice_get_ptr(result, 1));
    ASSERT_EQ(36, *(int *)slice_get_ptr(result, 2));

    growing_arena_destroy(a);
}

/* ---- iter_chain -------------------------------------------------------- */

TEST(iter_chain_concatenates)
{
    int a[] = {1, 2, 3};
    int b[] = {4, 5};
    slice_t sa = {a, 3, sizeof(int)};
    slice_t sb = {b, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t al = scratch_allocator(&sc);
    size_t n = iter_count(
        iter_chain(iter_from_slice(sa, al), iter_from_slice(sb, al)));
    ASSERT_EQ(5, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_chain_collects_in_order)
{
    int a[] = {1, 2};
    int b[] = {3, 4};
    slice_t sa = {a, 2, sizeof(int)};
    slice_t sb = {b, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    allocator_t al = growing_arena_allocator(arena);
    slice_t result = iter_collect(
        iter_chain(iter_from_slice(sa, al), iter_from_slice(sb, al)), al);
    ASSERT_EQ(4, result.len);
    ASSERT_EQ(1, *(int *)slice_get_ptr(result, 0));
    ASSERT_EQ(4, *(int *)slice_get_ptr(result, 3));
    growing_arena_destroy(arena);
}

TEST(iter_chain_empty_first)
{
    int b[] = {7, 8};
    slice_t sa = {NULL, 0, sizeof(int)};
    slice_t sb = {b, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t al = scratch_allocator(&sc);
    ASSERT_EQ(
        2,
        iter_count(
            iter_chain(iter_from_slice(sa, al), iter_from_slice(sb, al))));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_zip ---------------------------------------------------------- */

TEST(iter_zip_pairs_elements)
{
    int as[] = {1, 2, 3};
    char bs[] = {'a', 'b', 'c'};
    slice_t sa = {as, 3, sizeof(int)};
    slice_t sb = {bs, 3, sizeof(char)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    allocator_t al = growing_arena_allocator(arena);
    iter_t z = iter_zip(iter_from_slice(sa, al), iter_from_slice(sb, al));
    ASSERT_EQ(sizeof(int) + sizeof(char), z.elem_size);
    char buf[sizeof(int) + sizeof(char)];
    int count = 0;
    while (z.next(&z, buf))
    {
        int iv;
        memcpy(&iv, buf, sizeof(int));
        char cv;
        memcpy(&cv, buf + sizeof(int), sizeof(char));
        ASSERT_EQ(count + 1, iv);
        ASSERT_EQ('a' + count, cv);
        count++;
    }
    ASSERT_EQ(3, count);
    iter_destroy(&z);
    growing_arena_destroy(arena);
}

TEST(iter_zip_stops_at_shorter)
{
    int as[] = {1, 2, 3, 4};
    int bs[] = {10, 20};
    slice_t sa = {as, 4, sizeof(int)};
    slice_t sb = {bs, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t al = scratch_allocator(&sc);
    ASSERT_EQ(
        2,
        iter_count(iter_zip(iter_from_slice(sa, al), iter_from_slice(sb, al))));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_sort --------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

TEST(iter_sort_ascending)
{
    int data[] = {5, 1, 4, 2, 3};
    slice_t s = {data, 5, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    slice_t result = iter_sort(
        iter_from_slice(s, growing_arena_allocator(arena)),
        int_cmp,
        growing_arena_allocator(arena));
    ASSERT_EQ(5, result.len);
    for (size_t i = 0; i < result.len; i++)
    {
        ASSERT_EQ((int)(i + 1), *(int *)slice_get_ptr(result, i));
    }
    growing_arena_destroy(arena);
}

TEST(iter_sort_already_sorted)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    slice_t result = iter_sort(
        iter_from_slice(s, growing_arena_allocator(arena)),
        int_cmp,
        growing_arena_allocator(arena));
    ASSERT_EQ(1, *(int *)slice_get_ptr(result, 0));
    ASSERT_EQ(3, *(int *)slice_get_ptr(result, 2));
    growing_arena_destroy(arena);
}

/* ---- iter_find --------------------------------------------------------- */

TEST(iter_find_returns_first_match)
{
    int data[] = {1, 15, 22, 18};
    slice_t s = {data, 4, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int out = 0;
    int found =
        iter_find(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL, &out);
    ASSERT_TRUE(found);
    ASSERT_EQ(15, out);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_find_not_found)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int found =
        iter_find(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL, NULL);
    ASSERT_FALSE(found);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_any / iter_all ----------------------------------------------- */

TEST(iter_any_true_when_match_exists)
{
    int data[] = {1, 2, 20};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    ASSERT_TRUE(
        iter_any(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_any_false_when_no_match)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    ASSERT_FALSE(
        iter_any(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_all_true_when_all_match)
{
    int data[] = {11, 22, 33};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    ASSERT_TRUE(
        iter_all(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_all_false_when_one_fails)
{
    int data[] = {11, 22, 5};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    ASSERT_FALSE(
        iter_all(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_all_vacuously_true_for_empty)
{
    slice_t s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    ASSERT_TRUE(
        iter_all(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_enumerate ---------------------------------------------------- */

TEST(iter_enumerate_indices_and_values)
{
    int data[] = {10, 20, 30};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    iter_t it = iter_enumerate(iter_from_slice(s, scratch_allocator(&sc)));
    enum_entry_t e;
    it.next(&it, &e);
    ASSERT_EQ(0, e.index);
    ASSERT_EQ(10, *(int *)e.elem);
    it.next(&it, &e);
    ASSERT_EQ(1, e.index);
    ASSERT_EQ(20, *(int *)e.elem);
    it.next(&it, &e);
    ASSERT_EQ(2, e.index);
    ASSERT_EQ(30, *(int *)e.elem);
    ASSERT_FALSE(it.next(&it, &e));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_enumerate_empty)
{
    slice_t s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    iter_t it = iter_enumerate(iter_from_slice(s, scratch_allocator(&sc)));
    enum_entry_t e;
    ASSERT_FALSE(it.next(&it, &e));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_window ------------------------------------------------------- */

TEST(iter_window_basic)
{
    int data[] = {1, 2, 3, 4, 5};
    slice_t s = {data, 5, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    iter_t it = iter_window(iter_from_slice(s, scratch_allocator(&sc)), 3);
    slice_t w;
    /* window [1,2,3] */
    ASSERT_TRUE(it.next(&it, &w));
    ASSERT_EQ(3, w.len);
    ASSERT_EQ(1, *(int *)slice_get_ptr(w, 0));
    ASSERT_EQ(2, *(int *)slice_get_ptr(w, 1));
    ASSERT_EQ(3, *(int *)slice_get_ptr(w, 2));
    /* window [2,3,4] */
    ASSERT_TRUE(it.next(&it, &w));
    ASSERT_EQ(2, *(int *)slice_get_ptr(w, 0));
    ASSERT_EQ(4, *(int *)slice_get_ptr(w, 2));
    /* window [3,4,5] */
    ASSERT_TRUE(it.next(&it, &w));
    ASSERT_EQ(3, *(int *)slice_get_ptr(w, 0));
    ASSERT_EQ(5, *(int *)slice_get_ptr(w, 2));
    ASSERT_FALSE(it.next(&it, &w));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_window_source_too_short)
{
    int data[] = {1, 2};
    slice_t s = {data, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    iter_t it = iter_window(iter_from_slice(s, scratch_allocator(&sc)), 3);
    slice_t w;
    ASSERT_FALSE(it.next(&it, &w));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_window_zero_yields_nothing)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    ASSERT_EQ(
        0u,
        iter_count(iter_window(iter_from_slice(s, scratch_allocator(&sc)), 0)));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_chunks_zero_yields_nothing)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    ASSERT_EQ(
        0u,
        iter_count(iter_chunks(iter_from_slice(s, scratch_allocator(&sc)), 0)));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_chunks ------------------------------------------------------- */

TEST(iter_chunks_even)
{
    int data[] = {1, 2, 3, 4, 5, 6};
    slice_t s = {data, 6, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    iter_t it = iter_chunks(iter_from_slice(s, scratch_allocator(&sc)), 2);
    slice_t c;
    ASSERT_TRUE(it.next(&it, &c));
    ASSERT_EQ(2, c.len);
    ASSERT_EQ(1, *(int *)slice_get_ptr(c, 0));
    ASSERT_EQ(2, *(int *)slice_get_ptr(c, 1));
    ASSERT_TRUE(it.next(&it, &c));
    ASSERT_EQ(2, c.len);
    ASSERT_EQ(3, *(int *)slice_get_ptr(c, 0));
    ASSERT_EQ(4, *(int *)slice_get_ptr(c, 1));
    ASSERT_TRUE(it.next(&it, &c));
    ASSERT_EQ(2, c.len);
    ASSERT_EQ(5, *(int *)slice_get_ptr(c, 0));
    ASSERT_EQ(6, *(int *)slice_get_ptr(c, 1));
    ASSERT_FALSE(it.next(&it, &c));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_chunks_remainder)
{
    int data[] = {1, 2, 3, 4, 5};
    slice_t s = {data, 5, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    iter_t it = iter_chunks(iter_from_slice(s, scratch_allocator(&sc)), 2);
    slice_t c;
    it.next(&it, &c);
    ASSERT_EQ(2, c.len);
    it.next(&it, &c);
    ASSERT_EQ(2, c.len);
    ASSERT_TRUE(it.next(&it, &c));
    ASSERT_EQ(1, c.len); /* remainder */
    ASSERT_EQ(5, *(int *)slice_get_ptr(c, 0));
    ASSERT_FALSE(it.next(&it, &c));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_flat_map ----------------------------------------------------- */

/* Expands each int n into n copies of n: [1,2,3] -> [1, 2,2, 3,3,3] */
static void repeat_n(const void *elem, iter_t *out, void *ctx)
{
    int n = *(const int *)elem;
    allocator_t *alloc = (allocator_t *)ctx;
    /* Build a small vec_t of n copies and return an iter over it */
    vec_t *v = vec_create(sizeof(int), *alloc);
    for (int i = 0; i < n; i++)
    {
        vec_push(v, &n);
    }
    *out = vec_iter(v);
}

TEST(iter_flat_map_expand)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 1024);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t alloc = scratch_allocator(&sc);
    iter_t it =
        iter_flat_map(iter_from_slice(s, alloc), repeat_n, &alloc, sizeof(int));
    int expected[] = {1, 2, 2, 3, 3, 3};
    int val;
    for (int i = 0; i < 6; i++)
    {
        ASSERT_TRUE(it.next(&it, &val));
        ASSERT_EQ(expected[i], val);
    }
    ASSERT_FALSE(it.next(&it, &val));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_flat_map_empty_source)
{
    slice_t s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t alloc = scratch_allocator(&sc);
    iter_t it =
        iter_flat_map(iter_from_slice(s, alloc), repeat_n, &alloc, sizeof(int));
    int val;
    ASSERT_FALSE(it.next(&it, &val));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_min / iter_max ----------------------------------------------- */

TEST(iter_min_basic)
{
    int data[] = {5, 3, 8, 1, 9, 2};
    slice_t s = {data, 6, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int result;
    ASSERT_TRUE(
        iter_min(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, &result));
    ASSERT_EQ(1, result);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_max_basic)
{
    int data[] = {5, 3, 8, 1, 9, 2};
    slice_t s = {data, 6, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int result;
    ASSERT_TRUE(
        iter_max(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, &result));
    ASSERT_EQ(9, result);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_min_max_empty_returns_0)
{
    slice_t s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    ASSERT_FALSE(
        iter_min(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, NULL));
    growing_arena_scratch_begin(&sc, arena);
    ASSERT_FALSE(
        iter_max(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_min_max_single_element)
{
    int data[] = {42};
    slice_t s = {data, 1, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int mn, mx;
    iter_min(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, &mn);
    growing_arena_scratch_begin(&sc, arena);
    iter_max(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, &mx);
    ASSERT_EQ(42, mn);
    ASSERT_EQ(42, mx);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_take_while --------------------------------------------------- */

TEST(iter_take_while_basic)
{
    /* yields the leading prefix where pred holds */
    int data[] = {15, 22, 8, 30};
    slice_t s = {data, 4, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    slice_t result = iter_collect(
        iter_take_while(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL),
        scratch_allocator(&sc));
    /* gt10: 15 ✓, 22 ✓, 8 ✗ → stop */
    ASSERT_EQ(2, result.len);
    ASSERT_EQ(15, *(int *)slice_get_ptr(result, 0));
    ASSERT_EQ(22, *(int *)slice_get_ptr(result, 1));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_take_while_none_match)
{
    /* first element fails pred → yields nothing */
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_take_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    ASSERT_EQ(0, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_take_while_all_match)
{
    /* all elements satisfy pred → yields all */
    int data[] = {11, 22, 33};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_take_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    ASSERT_EQ(3, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_take_while_empty_source)
{
    slice_t s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_take_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    ASSERT_EQ(0, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_skip_while --------------------------------------------------- */

TEST(iter_skip_while_basic)
{
    /* skips leading prefix where pred holds, then yields the rest */
    int data[] = {15, 22, 8, 30};
    slice_t s = {data, 4, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    slice_t result = iter_collect(
        iter_skip_while(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL),
        scratch_allocator(&sc));
    /* gt10: 15 skip, 22 skip, 8 → yield, 30 → yield */
    ASSERT_EQ(2, result.len);
    ASSERT_EQ(8, *(int *)slice_get_ptr(result, 0));
    ASSERT_EQ(30, *(int *)slice_get_ptr(result, 1));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_skip_while_none_match)
{
    /* first element fails pred → yields all */
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_skip_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    ASSERT_EQ(3, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_skip_while_all_match)
{
    /* all elements satisfy pred → yields nothing */
    int data[] = {11, 22, 33};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_skip_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    ASSERT_EQ(0, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_skip_while_empty_source)
{
    slice_t s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_skip_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    ASSERT_EQ(0, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_skip_while_yields_all_after_first_miss)
{
    /* elements after the first miss should be yielded even if pred would match
     */
    int data[] = {15, 3, 22, 8};
    slice_t s = {data, 4, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    slice_t result = iter_collect(
        iter_skip_while(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL),
        scratch_allocator(&sc));
    /* gt10: 15 skip, 3 → yield; then 22 and 8 must also be yielded */
    ASSERT_EQ(3, result.len);
    ASSERT_EQ(3, *(int *)slice_get_ptr(result, 0));
    ASSERT_EQ(22, *(int *)slice_get_ptr(result, 1));
    ASSERT_EQ(8, *(int *)slice_get_ptr(result, 2));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_generate ----------------------------------------------------- */

/* Counts up from an int state; stops when it reaches 5 */
static bool count_up(void *out, void *ctx)
{
    int *n = (int *)ctx;
    if (*n >= 5)
    {
        return false;
    }
    *(int *)out = (*n)++;
    return true;
}

TEST(iter_generate_basic)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int state = 0;
    slice_t result = iter_collect(
        iter_generate(count_up, &state, sizeof(int), scratch_allocator(&sc)),
        scratch_allocator(&sc));
    ASSERT_EQ(5, result.len);
    for (size_t i = 0; i < result.len; i++)
    {
        ASSERT_EQ((int)i, *(int *)slice_get_ptr(result, i));
    }
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_generate_empty_when_fn_false_immediately)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int state = 5; /* already at limit — fn returns false immediately */
    size_t n = iter_count(
        iter_generate(count_up, &state, sizeof(int), scratch_allocator(&sc)));
    ASSERT_EQ(0, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_generate_with_take)
{
    /* Generate is infinite without take; take limits it */
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int state = 0;
    /* reuse count_up but stop at 5 naturally; just verify take works too */
    size_t n = iter_count(iter_take(
        iter_generate(count_up, &state, sizeof(int), scratch_allocator(&sc)),
        3));
    ASSERT_EQ(3, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_range -------------------------------------------------------- */

TEST(iter_range_basic)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    slice_t result = iter_collect(
        iter_range(0, 5, 1, scratch_allocator(&sc)), scratch_allocator(&sc));
    ASSERT_EQ(5, result.len);
    for (size_t i = 0; i < result.len; i++)
    {
        ASSERT_EQ((long long)i, *(long long *)slice_get_ptr(result, i));
    }
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_range_step_two)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    slice_t result = iter_collect(
        iter_range(0, 10, 2, scratch_allocator(&sc)), scratch_allocator(&sc));
    /* 0, 2, 4, 6, 8 */
    ASSERT_EQ(5, result.len);
    ASSERT_EQ(0LL, *(long long *)slice_get_ptr(result, 0));
    ASSERT_EQ(8LL, *(long long *)slice_get_ptr(result, 4));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_range_negative_step)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    slice_t result = iter_collect(
        iter_range(5, 0, -1, scratch_allocator(&sc)), scratch_allocator(&sc));
    /* 5, 4, 3, 2, 1 */
    ASSERT_EQ(5, result.len);
    ASSERT_EQ(5LL, *(long long *)slice_get_ptr(result, 0));
    ASSERT_EQ(1LL, *(long long *)slice_get_ptr(result, 4));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_range_empty_when_start_equals_end)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_range(3, 3, 1, scratch_allocator(&sc)));
    ASSERT_EQ(0, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_range_empty_wrong_direction)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    /* positive step but start > end */
    size_t n = iter_count(iter_range(5, 0, 1, scratch_allocator(&sc)));
    ASSERT_EQ(0, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter_range_zero_step_returns_empty)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_range(0, 10, 0, scratch_allocator(&sc)));
    ASSERT_EQ(0, n);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_peekable ----------------------------------------------------- */

TEST(iter_peekable_peek_does_not_consume)
{
    int data[] = {10, 20, 30};
    slice_t s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = iter_peekable(iter_from_slice(s, scratch_allocator(&sc)));
    int peeked, got;
    ASSERT_TRUE(iter_peek(&it, &peeked));
    ASSERT_EQ(10, peeked);
    ASSERT_TRUE(it.next(&it, &got));
    ASSERT_EQ(10, got); /* peek did not consume */
    ASSERT_TRUE(it.next(&it, &got));
    ASSERT_EQ(20, got);
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_peekable_peek_at_end_returns_false)
{
    int data[] = {1};
    slice_t s = {data, 1, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = iter_peekable(iter_from_slice(s, scratch_allocator(&sc)));
    int got;
    it.next(&it, &got); /* consume the only element */
    ASSERT_FALSE(iter_peek(&it, &got));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_peekable_multiple_peeks_return_same)
{
    int data[] = {42, 99};
    slice_t s = {data, 2, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = iter_peekable(iter_from_slice(s, scratch_allocator(&sc)));
    int p1, p2;
    ASSERT_TRUE(iter_peek(&it, &p1));
    ASSERT_TRUE(iter_peek(&it, &p2));
    ASSERT_EQ(p2, p1); /* repeated peek returns same value */
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* ---- iter_dedup -------------------------------------------------------- */

static int int_cmp_dedup(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

TEST(iter_dedup_removes_consecutive_duplicates)
{
    int data[] = {1, 1, 2, 3, 3, 3, 4};
    slice_t s = {data, 7, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    slice_t result = iter_collect(
        iter_dedup(iter_from_slice(s, scratch_allocator(&sc)), int_cmp_dedup),
        scratch_allocator(&sc));
    ASSERT_EQ(4, result.len);
    ASSERT_EQ(1, *(int *)slice_get_ptr(result, 0));
    ASSERT_EQ(2, *(int *)slice_get_ptr(result, 1));
    ASSERT_EQ(3, *(int *)slice_get_ptr(result, 2));
    ASSERT_EQ(4, *(int *)slice_get_ptr(result, 3));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_dedup_non_consecutive_duplicates_kept)
{
    int data[] = {1, 2, 1, 2};
    slice_t s = {data, 4, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(
        iter_dedup(iter_from_slice(s, scratch_allocator(&sc)), int_cmp_dedup));
    ASSERT_EQ(4, n); /* no adjacent duplicates */
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter_sort_then_dedup_unique_values)
{
    int data[] = {3, 1, 2, 1, 3, 2};
    slice_t s = {data, 6, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    slice_t result = iter_collect(
        iter_dedup(
            iter_from_slice(
                iter_sort(
                    iter_from_slice(s, scratch_allocator(&sc)),
                    int_cmp_dedup,
                    scratch_allocator(&sc)),
                scratch_allocator(&sc)),
            int_cmp_dedup),
        scratch_allocator(&sc));
    ASSERT_EQ(3, result.len);
    ASSERT_EQ(1, *(int *)slice_get_ptr(result, 0));
    ASSERT_EQ(2, *(int *)slice_get_ptr(result, 1));
    ASSERT_EQ(3, *(int *)slice_get_ptr(result, 2));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* ---- failed iterators ---------------------------------------------------
 * Every constructor returns the zero iter_t when it cannot make one.  These
 * tests pin down what happens to it: it passes through adaptors, it is safe
 * everywhere, and nothing leaks on the way. */

/* An allocator that counts live allocations and fails once its budget is
 * spent: walks every allocation failure, and shows what was not freed. */
typedef struct
{
    size_t budget; /* allocations (alloc or realloc) still allowed */
    int live;      /* allocated and not yet freed */
} counting_t;

static void *counting_alloc(void *ctx, size_t size, size_t align)
{
    (void)align;
    counting_t *c = ctx;
    if (c->budget == 0)
    {
        return NULL;
    }
    c->budget--;
    void *p = malloc(size);
    c->live += p != NULL;
    return p;
}

static void *counting_realloc(
    void *ctx, void *ptr, size_t old_size, size_t new_size, size_t align)
{
    (void)old_size;
    (void)align;
    counting_t *c = ctx;
    if (c->budget == 0)
    {
        return NULL;
    }
    c->budget--;
    void *p = realloc(ptr, new_size);
    c->live += p != NULL && ptr == NULL;
    return p;
}

static void counting_free(void *ctx, void *ptr, size_t size)
{
    (void)size;
    counting_t *c = ctx;
    c->live -= ptr != NULL;
    free(ptr);
}

static const allocator_vtable_t counting_vtable = {
    counting_alloc, counting_realloc, counting_free};

static allocator_t counting(counting_t *c)
{
    return (allocator_t){&counting_vtable, c};
}

static int failed_data[] = {1, 2, 3, 4, 5, 20, 30};
#define FAILED_LEN 7

static iter_t src(counting_t *c)
{
    slice_t s = {failed_data, FAILED_LEN, sizeof(int)};
    return iter_from_slice(s, counting(c));
}

static int int_cmp_failed(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static void flat_twice(const void *elem, iter_t *out, void *ctx)
{
    (void)ctx;
    (void)elem;
    *out = iter_range(0, 2, 1, ALLOCATOR_NULL); /* fails: no allocator */
}

static iter_t ad_filter(iter_t s)
{
    return iter_filter(s, gt10, NULL);
}
static iter_t ad_map(iter_t s)
{
    return iter_map(s, double_it, NULL, sizeof(int));
}
static iter_t ad_take(iter_t s)
{
    return iter_take(s, 3);
}
static iter_t ad_take_while(iter_t s)
{
    return iter_take_while(s, gt10, NULL);
}
static iter_t ad_skip(iter_t s)
{
    return iter_skip(s, 2);
}
static iter_t ad_skip_while(iter_t s)
{
    return iter_skip_while(s, gt10, NULL);
}
static iter_t ad_enumerate(iter_t s)
{
    return iter_enumerate(s);
}
static iter_t ad_window(iter_t s)
{
    return iter_window(s, 2);
}
static iter_t ad_chunks(iter_t s)
{
    return iter_chunks(s, 2);
}
static iter_t ad_peekable(iter_t s)
{
    return iter_peekable(s);
}
static iter_t ad_dedup(iter_t s)
{
    return iter_dedup(s, int_cmp_failed);
}
static iter_t ad_flat_map(iter_t s)
{
    return iter_flat_map(s, flat_twice, NULL, sizeof(long long));
}
static iter_t ad_chain_left(iter_t s)
{
    return iter_chain(s, (iter_t){0});
}
static iter_t ad_chain_right(iter_t s)
{
    return iter_chain((iter_t){0}, s);
}
static iter_t ad_zip_left(iter_t s)
{
    return iter_zip(s, (iter_t){0});
}
static iter_t ad_zip_right(iter_t s)
{
    return iter_zip((iter_t){0}, s);
}

typedef iter_t (*adaptor_fn)(iter_t);

static const adaptor_fn single_adaptors[] = {
    ad_filter,
    ad_map,
    ad_take,
    ad_take_while,
    ad_skip,
    ad_skip_while,
    ad_enumerate,
    ad_window,
    ad_chunks,
    ad_peekable,
    ad_dedup,
    ad_flat_map};
#define N_SINGLE (sizeof single_adaptors / sizeof single_adaptors[0])

TEST(failed_iterator_is_safe_everywhere)
{
    iter_t z = {0};
    ASSERT_FALSE(iter_is_valid(&z));
    ASSERT_FALSE(iter_is_valid(NULL));
    int x = 42;
    ASSERT_FALSE(iter_next(&z, &x));
    ASSERT_EQ(42, x); /* out untouched */
    ASSERT_FALSE(iter_next(NULL, &x));
    iter_destroy(&z); /* no-op */
}

TEST(terminals_treat_a_failed_iterator_as_empty)
{
    iter_t z = {0};
    int acc = 7, out = 99;
    ASSERT_EQ(0, iter_count(z));
    slice_t s = iter_collect(z, ALLOCATOR_NULL);
    ASSERT_NULL(s.ptr);
    ASSERT_EQ(0, s.len);
    s = iter_sort(z, int_cmp_failed, ALLOCATOR_NULL);
    ASSERT_NULL(s.ptr);
    int *cursor = &out;
    iter_foreach(z, push_to_arr, &cursor);
    ASSERT_EQ(99, out); /* never called */
    iter_reduce(z, &acc, sum_combine, NULL);
    ASSERT_EQ(7, acc);
    ASSERT_FALSE(iter_find(z, gt10, NULL, &out));
    ASSERT_FALSE(iter_any(z, gt10, NULL));
    ASSERT_FALSE(iter_all(z, gt10, NULL)); /* not "vacuously all" */
    ASSERT_FALSE(iter_min(z, int_cmp_failed, &out));
    ASSERT_FALSE(iter_max(z, int_cmp_failed, &out));
}

/* A failed source fails the adaptor: one check at the end of a pipeline. */
TEST(adaptors_pass_a_failed_source_on)
{
    for (size_t i = 0; i < N_SINGLE; i++)
    {
        iter_t it = single_adaptors[i]((iter_t){0});
        ASSERT_FALSE(iter_is_valid(&it));
    }
    iter_t both = iter_chain((iter_t){0}, (iter_t){0});
    ASSERT_FALSE(iter_is_valid(&both));
    both = iter_zip((iter_t){0}, (iter_t){0});
    ASSERT_FALSE(iter_is_valid(&both));
}

/* chain / zip with one failed side: the other side is destroyed, not
 * leaked. */
TEST(chain_and_zip_destroy_the_good_side)
{
    adaptor_fn two[] = {
        ad_chain_left, ad_chain_right, ad_zip_left, ad_zip_right};
    for (size_t i = 0; i < 4; i++)
    {
        counting_t c = {100, 0};
        iter_t it = two[i](src(&c));
        ASSERT_FALSE(iter_is_valid(&it));
        ASSERT_EQ(0, c.live);
    }
}

/* Every adaptor, at every allocation it makes: when one fails, the adaptor
 * is the failed iterator, and nothing is left allocated — its own partial
 * state and the source are both released. */
TEST(adaptor_out_of_memory_releases_everything)
{
    for (size_t i = 0; i < N_SINGLE; i++)
    {
        for (size_t budget = 1;; budget++)
        {
            counting_t c = {budget, 0};
            iter_t it = single_adaptors[i](src(&c)); /* source takes 1 */
            if (iter_is_valid(&it))
            {
                iter_destroy(&it);
                ASSERT_EQ(0, c.live);
                break;
            }
            ASSERT_EQ(0, c.live);
            ASSERT_TRUE(budget < 10);
        }
    }
}

/* A failure in the middle of a pipeline reaches its end. */
TEST(a_failure_reaches_the_end_of_the_pipeline)
{
    counting_t c = {2, 0}; /* source and take; filter fails */
    iter_t it = iter_map(
        iter_filter(iter_take(src(&c), 5), gt10, NULL),
        double_it,
        NULL,
        sizeof(int));
    ASSERT_FALSE(iter_is_valid(&it));
    ASSERT_EQ(0, c.live);
    ASSERT_EQ(0, iter_count(it));
}

/* iter_collect running out of memory half-way gives {NULL, 0}, not the
 * part it had — that would look like the whole sequence. */
TEST(collect_out_of_memory_is_not_a_partial_result)
{
    int big[100];
    for (int i = 0; i < 100; i++)
    {
        big[i] = i;
    }
    counting_t sc = {100, 0};
    counting_t rc = {2, 0}; /* the first buffer and one growth, no more */
    slice_t s = iter_collect(
        iter_from_slice((slice_t){big, 100, sizeof(int)}, counting(&sc)),
        counting(&rc));
    ASSERT_NULL(s.ptr);
    ASSERT_EQ(0, s.len);
    ASSERT_EQ(0, rc.live);
    ASSERT_EQ(0, sc.live); /* the source was destroyed */
}

/* The *_extend functions say what terminals cannot: SEQC_INVALID for a
 * failed iterator, or one of the wrong element size. */
TEST(extend_refuses_a_failed_or_mismatched_iterator)
{
    growing_arena_t arena;
    growing_arena_init(&arena, 1024);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(&arena));
    int one = 1;
    ASSERT_EQ(SEQC_OK, vec_push(v, &one));

    ASSERT_EQ(SEQC_INVALID, vec_extend(v, (iter_t){0}));
    ASSERT_EQ(1, vec_len(v));

    counting_t c = {100, 0};
    iter_t wide = iter_range(0, 5, 1, counting(&c)); /* long long elements */
    ASSERT_EQ(SEQC_INVALID, vec_extend(v, wide));
    ASSERT_EQ(1, vec_len(v));
    ASSERT_EQ(0, c.live); /* the iterator was destroyed */

    ASSERT_EQ(SEQC_OK, vec_extend(v, src(&c)));
    ASSERT_EQ(1 + FAILED_LEN, vec_len(v));
    ASSERT_EQ(0, c.live);
    growing_arena_destroy(&arena);
}

TEST(string_join_of_a_failed_iterator_is_null)
{
    string_t j = string_join((iter_t){0}, STRING_LIT(","), ALLOCATOR_NULL);
    ASSERT_NULL(j.ptr);
    ASSERT_EQ(0, j.len);
}

/* flat_map's sub-iterators are made while it runs; one that fails counts
 * as empty instead of crashing. */
TEST(flat_map_skips_a_failed_sub_iterator)
{
    counting_t c = {100, 0};
    ASSERT_EQ(0, iter_count(ad_flat_map(src(&c))));
    ASSERT_EQ(0, c.live);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "iter_test");
}
