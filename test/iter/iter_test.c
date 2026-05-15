#include <gtest/gtest.h>

extern "C"
{
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/iter.h"
#include "seqc/vec.h"
}

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

TEST(iter, from_slice_count)
{
    int data[] = {1, 2, 3, 4, 5};
    Slice s = {data, 5, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    EXPECT_EQ(iter_count(iter_from_slice(s, scratch_allocator(&sc))), 5);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, from_slice_empty)
{
    Slice s = {NULL, 0, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    EXPECT_EQ(iter_count(iter_from_slice(s, scratch_allocator(&sc))), 0);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, filter_keeps_matching)
{
    int data[] = {3, 15, 7, 22, 1, 18};
    Slice s = {data, 6, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(
        iter_filter(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    EXPECT_EQ(n, 3);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, filter_none_match)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(
        iter_filter(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    EXPECT_EQ(n, 0);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, map_doubles_values)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);

    Slice result = iter_collect(
        iter_map(
            iter_from_slice(s, growing_arena_allocator(a)),
            double_it,
            NULL,
            sizeof(int)),
        growing_arena_allocator(a));

    EXPECT_EQ(result.len, 3);
    EXPECT_EQ(*(int *)slice_get(result, 0), 2);
    EXPECT_EQ(*(int *)slice_get(result, 1), 4);
    EXPECT_EQ(*(int *)slice_get(result, 2), 6);

    growing_arena_destroy(a);
}

TEST(iter, collect_produces_correct_slice)
{
    int data[] = {10, 20, 30, 40};
    Slice s = {data, 4, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);

    Slice result = iter_collect(
        iter_from_slice(s, growing_arena_allocator(a)),
        growing_arena_allocator(a));

    EXPECT_EQ(result.len, 4);
    for (size_t i = 0; i < result.len; i++)
        EXPECT_EQ(*(int *)slice_get(result, i), data[i]);

    growing_arena_destroy(a);
}

TEST(iter, collect_empty_gives_null_ptr)
{
    Slice s = {NULL, 0, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);

    Slice result = iter_collect(
        iter_from_slice(s, growing_arena_allocator(a)),
        growing_arena_allocator(a));

    EXPECT_EQ(result.len, 0);
    EXPECT_EQ(result.ptr, nullptr);

    growing_arena_destroy(a);
}

TEST(iter, take_limits_output)
{
    int data[] = {1, 2, 3, 4, 5};
    Slice s = {data, 5, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    EXPECT_EQ(
        iter_count(iter_take(iter_from_slice(s, scratch_allocator(&sc)), 3)),
        3);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, take_more_than_available)
{
    int data[] = {1, 2};
    Slice s = {data, 2, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    EXPECT_EQ(
        iter_count(iter_take(iter_from_slice(s, scratch_allocator(&sc)), 100)),
        2);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, skip_drops_first_n)
{
    int data[] = {1, 2, 3, 4, 5};
    Slice s = {data, 5, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    EXPECT_EQ(
        iter_count(iter_skip(iter_from_slice(s, scratch_allocator(&sc)), 3)),
        2);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, skip_all)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    EXPECT_EQ(
        iter_count(iter_skip(iter_from_slice(s, scratch_allocator(&sc)), 10)),
        0);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, reduce_sum)
{
    int data[] = {1, 2, 3, 4, 5};
    Slice s = {data, 5, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    int sum = 0;
    iter_reduce(
        iter_from_slice(s, scratch_allocator(&sc)), &sum, sum_combine, NULL);
    EXPECT_EQ(sum, 15);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, foreach_visits_all)
{
    int data[] = {10, 20, 30};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    int out[3] = {0};
    int *ptr = out;
    iter_foreach(iter_from_slice(s, scratch_allocator(&sc)), push_to_arr, &ptr);
    EXPECT_EQ(out[0], 10);
    EXPECT_EQ(out[1], 20);
    EXPECT_EQ(out[2], 30);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, filter_map_chain)
{
    int data[] = {3, 15, 7, 22, 1, 18};
    Slice s = {data, 6, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);

    Slice result = iter_collect(
        iter_map(
            iter_filter(
                iter_from_slice(s, growing_arena_allocator(a)), gt10, NULL),
            double_it,
            NULL,
            sizeof(int)),
        growing_arena_allocator(a));

    /* 15→30, 22→44, 18→36 */
    EXPECT_EQ(result.len, 3);
    EXPECT_EQ(*(int *)slice_get(result, 0), 30);
    EXPECT_EQ(*(int *)slice_get(result, 1), 44);
    EXPECT_EQ(*(int *)slice_get(result, 2), 36);

    growing_arena_destroy(a);
}

/* ---- iter_chain -------------------------------------------------------- */

TEST(iter, chain_concatenates)
{
    int a[] = {1, 2, 3};
    int b[] = {4, 5};
    Slice sa = {a, 3, sizeof(int)};
    Slice sb = {b, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t al = scratch_allocator(&sc);
    size_t n = iter_count(
        iter_chain(iter_from_slice(sa, al), iter_from_slice(sb, al)));
    EXPECT_EQ(n, 5);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, chain_collects_in_order)
{
    int a[] = {1, 2};
    int b[] = {3, 4};
    Slice sa = {a, 2, sizeof(int)};
    Slice sb = {b, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    allocator_t al = growing_arena_allocator(arena);
    Slice result = iter_collect(
        iter_chain(iter_from_slice(sa, al), iter_from_slice(sb, al)), al);
    EXPECT_EQ(result.len, 4);
    EXPECT_EQ(*(int *)slice_get(result, 0), 1);
    EXPECT_EQ(*(int *)slice_get(result, 3), 4);
    growing_arena_destroy(arena);
}

TEST(iter, chain_empty_first)
{
    int b[] = {7, 8};
    Slice sa = {NULL, 0, sizeof(int)};
    Slice sb = {b, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t al = scratch_allocator(&sc);
    EXPECT_EQ(
        iter_count(
            iter_chain(iter_from_slice(sa, al), iter_from_slice(sb, al))),
        2);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_zip ---------------------------------------------------------- */

TEST(iter, zip_pairs_elements)
{
    int as[] = {1, 2, 3};
    char bs[] = {'a', 'b', 'c'};
    Slice sa = {as, 3, sizeof(int)};
    Slice sb = {bs, 3, sizeof(char)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    allocator_t al = growing_arena_allocator(arena);
    Iter z = iter_zip(iter_from_slice(sa, al), iter_from_slice(sb, al));
    EXPECT_EQ(z.elem_size, sizeof(int) + sizeof(char));
    char buf[sizeof(int) + sizeof(char)];
    int count = 0;
    while (z.next(&z, buf))
    {
        int iv;
        memcpy(&iv, buf, sizeof(int));
        char cv;
        memcpy(&cv, buf + sizeof(int), sizeof(char));
        EXPECT_EQ(iv, count + 1);
        EXPECT_EQ(cv, 'a' + count);
        count++;
    }
    EXPECT_EQ(count, 3);
    iter_drop(&z);
    growing_arena_destroy(arena);
}

TEST(iter, zip_stops_at_shorter)
{
    int as[] = {1, 2, 3, 4};
    int bs[] = {10, 20};
    Slice sa = {as, 4, sizeof(int)};
    Slice sb = {bs, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t al = scratch_allocator(&sc);
    EXPECT_EQ(
        iter_count(iter_zip(iter_from_slice(sa, al), iter_from_slice(sb, al))),
        2);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_sort --------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

TEST(iter, sort_ascending)
{
    int data[] = {5, 1, 4, 2, 3};
    Slice s = {data, 5, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    Slice result = iter_sort(
        iter_from_slice(s, growing_arena_allocator(arena)),
        int_cmp,
        growing_arena_allocator(arena));
    EXPECT_EQ(result.len, 5);
    for (size_t i = 0; i < result.len; i++)
        EXPECT_EQ(*(int *)slice_get(result, i), (int)(i + 1));
    growing_arena_destroy(arena);
}

TEST(iter, sort_already_sorted)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    Slice result = iter_sort(
        iter_from_slice(s, growing_arena_allocator(arena)),
        int_cmp,
        growing_arena_allocator(arena));
    EXPECT_EQ(*(int *)slice_get(result, 0), 1);
    EXPECT_EQ(*(int *)slice_get(result, 2), 3);
    growing_arena_destroy(arena);
}

/* ---- iter_find --------------------------------------------------------- */

TEST(iter, find_returns_first_match)
{
    int data[] = {1, 15, 22, 18};
    Slice s = {data, 4, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int out = 0;
    int found =
        iter_find(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL, &out);
    EXPECT_TRUE(found);
    EXPECT_EQ(out, 15);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, find_not_found)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int found =
        iter_find(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL, NULL);
    EXPECT_FALSE(found);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_any / iter_all ----------------------------------------------- */

TEST(iter, any_true_when_match_exists)
{
    int data[] = {1, 2, 20};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    EXPECT_TRUE(
        iter_any(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, any_false_when_no_match)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    EXPECT_FALSE(
        iter_any(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, all_true_when_all_match)
{
    int data[] = {11, 22, 33};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    EXPECT_TRUE(
        iter_all(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, all_false_when_one_fails)
{
    int data[] = {11, 22, 5};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    EXPECT_FALSE(
        iter_all(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, all_vacuously_true_for_empty)
{
    Slice s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    EXPECT_TRUE(
        iter_all(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_enumerate ---------------------------------------------------- */

TEST(iter, enumerate_indices_and_values)
{
    int data[] = {10, 20, 30};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Iter it = iter_enumerate(iter_from_slice(s, scratch_allocator(&sc)));
    EnumEntry e;
    it.next(&it, &e);
    EXPECT_EQ(e.index, 0);
    EXPECT_EQ(*(int *)e.elem, 10);
    it.next(&it, &e);
    EXPECT_EQ(e.index, 1);
    EXPECT_EQ(*(int *)e.elem, 20);
    it.next(&it, &e);
    EXPECT_EQ(e.index, 2);
    EXPECT_EQ(*(int *)e.elem, 30);
    EXPECT_FALSE(it.next(&it, &e));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, enumerate_empty)
{
    Slice s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Iter it = iter_enumerate(iter_from_slice(s, scratch_allocator(&sc)));
    EnumEntry e;
    EXPECT_FALSE(it.next(&it, &e));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_window ------------------------------------------------------- */

TEST(iter, window_basic)
{
    int data[] = {1, 2, 3, 4, 5};
    Slice s = {data, 5, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Iter it = iter_window(iter_from_slice(s, scratch_allocator(&sc)), 3);
    Slice w;
    /* window [1,2,3] */
    EXPECT_TRUE(it.next(&it, &w));
    EXPECT_EQ(w.len, 3);
    EXPECT_EQ(*(int *)slice_get(w, 0), 1);
    EXPECT_EQ(*(int *)slice_get(w, 1), 2);
    EXPECT_EQ(*(int *)slice_get(w, 2), 3);
    /* window [2,3,4] */
    EXPECT_TRUE(it.next(&it, &w));
    EXPECT_EQ(*(int *)slice_get(w, 0), 2);
    EXPECT_EQ(*(int *)slice_get(w, 2), 4);
    /* window [3,4,5] */
    EXPECT_TRUE(it.next(&it, &w));
    EXPECT_EQ(*(int *)slice_get(w, 0), 3);
    EXPECT_EQ(*(int *)slice_get(w, 2), 5);
    EXPECT_FALSE(it.next(&it, &w));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, window_source_too_short)
{
    int data[] = {1, 2};
    Slice s = {data, 2, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Iter it = iter_window(iter_from_slice(s, scratch_allocator(&sc)), 3);
    Slice w;
    EXPECT_FALSE(it.next(&it, &w));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_chunks ------------------------------------------------------- */

TEST(iter, chunks_even)
{
    int data[] = {1, 2, 3, 4, 5, 6};
    Slice s = {data, 6, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Iter it = iter_chunks(iter_from_slice(s, scratch_allocator(&sc)), 2);
    Slice c;
    EXPECT_TRUE(it.next(&it, &c));
    EXPECT_EQ(c.len, 2);
    EXPECT_EQ(*(int *)slice_get(c, 0), 1);
    EXPECT_EQ(*(int *)slice_get(c, 1), 2);
    EXPECT_TRUE(it.next(&it, &c));
    EXPECT_EQ(c.len, 2);
    EXPECT_EQ(*(int *)slice_get(c, 0), 3);
    EXPECT_EQ(*(int *)slice_get(c, 1), 4);
    EXPECT_TRUE(it.next(&it, &c));
    EXPECT_EQ(c.len, 2);
    EXPECT_EQ(*(int *)slice_get(c, 0), 5);
    EXPECT_EQ(*(int *)slice_get(c, 1), 6);
    EXPECT_FALSE(it.next(&it, &c));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, chunks_remainder)
{
    int data[] = {1, 2, 3, 4, 5};
    Slice s = {data, 5, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Iter it = iter_chunks(iter_from_slice(s, scratch_allocator(&sc)), 2);
    Slice c;
    it.next(&it, &c);
    EXPECT_EQ(c.len, 2);
    it.next(&it, &c);
    EXPECT_EQ(c.len, 2);
    EXPECT_TRUE(it.next(&it, &c));
    EXPECT_EQ(c.len, 1); /* remainder */
    EXPECT_EQ(*(int *)slice_get(c, 0), 5);
    EXPECT_FALSE(it.next(&it, &c));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_flat_map ----------------------------------------------------- */

/* Expands each int n into n copies of n: [1,2,3] -> [1, 2,2, 3,3,3] */
static void repeat_n(const void *elem, Iter *out, void *ctx)
{
    int n = *(const int *)elem;
    allocator_t *alloc = (allocator_t *)ctx;
    /* Build a small Vec of n copies and return an iter over it */
    Vec *v = vec_create(sizeof(int), *alloc);
    for (int i = 0; i < n; i++)
        vec_push(v, &n);
    *out = vec_iter(v);
}

TEST(iter, flat_map_expand)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 1024);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t alloc = scratch_allocator(&sc);
    Iter it =
        iter_flat_map(iter_from_slice(s, alloc), repeat_n, &alloc, sizeof(int));
    int expected[] = {1, 2, 2, 3, 3, 3};
    int val;
    for (int i = 0; i < 6; i++)
    {
        EXPECT_TRUE(it.next(&it, &val));
        EXPECT_EQ(val, expected[i]);
    }
    EXPECT_FALSE(it.next(&it, &val));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, flat_map_empty_source)
{
    Slice s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    allocator_t alloc = scratch_allocator(&sc);
    Iter it =
        iter_flat_map(iter_from_slice(s, alloc), repeat_n, &alloc, sizeof(int));
    int val;
    EXPECT_FALSE(it.next(&it, &val));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_min / iter_max ----------------------------------------------- */

TEST(iter, min_basic)
{
    int data[] = {5, 3, 8, 1, 9, 2};
    Slice s = {data, 6, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int result;
    EXPECT_TRUE(
        iter_min(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, &result));
    EXPECT_EQ(result, 1);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, max_basic)
{
    int data[] = {5, 3, 8, 1, 9, 2};
    Slice s = {data, 6, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int result;
    EXPECT_TRUE(
        iter_max(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, &result));
    EXPECT_EQ(result, 9);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, min_max_empty_returns_0)
{
    Slice s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    EXPECT_FALSE(
        iter_min(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, NULL));
    growing_arena_scratch_begin(&sc, arena);
    EXPECT_FALSE(
        iter_max(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, NULL));
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, min_max_single_element)
{
    int data[] = {42};
    Slice s = {data, 1, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int mn, mx;
    iter_min(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, &mn);
    growing_arena_scratch_begin(&sc, arena);
    iter_max(iter_from_slice(s, scratch_allocator(&sc)), int_cmp, &mx);
    EXPECT_EQ(mn, 42);
    EXPECT_EQ(mx, 42);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_take_while --------------------------------------------------- */

TEST(iter, take_while_basic)
{
    /* yields the leading prefix where pred holds */
    int data[] = {15, 22, 8, 30};
    Slice s = {data, 4, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Slice result = iter_collect(
        iter_take_while(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL),
        scratch_allocator(&sc));
    /* gt10: 15 ✓, 22 ✓, 8 ✗ → stop */
    EXPECT_EQ(result.len, 2);
    EXPECT_EQ(*(int *)slice_get(result, 0), 15);
    EXPECT_EQ(*(int *)slice_get(result, 1), 22);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, take_while_none_match)
{
    /* first element fails pred → yields nothing */
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_take_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    EXPECT_EQ(n, 0);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, take_while_all_match)
{
    /* all elements satisfy pred → yields all */
    int data[] = {11, 22, 33};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_take_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    EXPECT_EQ(n, 3);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, take_while_empty_source)
{
    Slice s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_take_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    EXPECT_EQ(n, 0);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_skip_while --------------------------------------------------- */

TEST(iter, skip_while_basic)
{
    /* skips leading prefix where pred holds, then yields the rest */
    int data[] = {15, 22, 8, 30};
    Slice s = {data, 4, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Slice result = iter_collect(
        iter_skip_while(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL),
        scratch_allocator(&sc));
    /* gt10: 15 skip, 22 skip, 8 → yield, 30 → yield */
    EXPECT_EQ(result.len, 2);
    EXPECT_EQ(*(int *)slice_get(result, 0), 8);
    EXPECT_EQ(*(int *)slice_get(result, 1), 30);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, skip_while_none_match)
{
    /* first element fails pred → yields all */
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_skip_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    EXPECT_EQ(n, 3);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, skip_while_all_match)
{
    /* all elements satisfy pred → yields nothing */
    int data[] = {11, 22, 33};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_skip_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    EXPECT_EQ(n, 0);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, skip_while_empty_source)
{
    Slice s = {NULL, 0, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_skip_while(
        iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL));
    EXPECT_EQ(n, 0);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, skip_while_yields_all_after_first_miss)
{
    /* elements after the first miss should be yielded even if pred would match
     */
    int data[] = {15, 3, 22, 8};
    Slice s = {data, 4, sizeof(int)};
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Slice result = iter_collect(
        iter_skip_while(iter_from_slice(s, scratch_allocator(&sc)), gt10, NULL),
        scratch_allocator(&sc));
    /* gt10: 15 skip, 3 → yield; then 22 and 8 must also be yielded */
    EXPECT_EQ(result.len, 3);
    EXPECT_EQ(*(int *)slice_get(result, 0), 3);
    EXPECT_EQ(*(int *)slice_get(result, 1), 22);
    EXPECT_EQ(*(int *)slice_get(result, 2), 8);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_generate ----------------------------------------------------- */

/* Counts up from an int state; stops when it reaches 5 */
static bool count_up(void *out, void *ctx)
{
    int *n = (int *)ctx;
    if (*n >= 5)
        return false;
    *(int *)out = (*n)++;
    return true;
}

TEST(iter, generate_basic)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int state = 0;
    Slice result = iter_collect(
        iter_generate(count_up, &state, sizeof(int), scratch_allocator(&sc)),
        scratch_allocator(&sc));
    EXPECT_EQ(result.len, 5);
    for (size_t i = 0; i < result.len; i++)
        EXPECT_EQ(*(int *)slice_get(result, i), (int)i);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, generate_empty_when_fn_false_immediately)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    int state = 5; /* already at limit — fn returns false immediately */
    size_t n = iter_count(
        iter_generate(count_up, &state, sizeof(int), scratch_allocator(&sc)));
    EXPECT_EQ(n, 0);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, generate_with_take)
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
    EXPECT_EQ(n, 3);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_range -------------------------------------------------------- */

TEST(iter, range_basic)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Slice result = iter_collect(
        iter_range(0, 5, 1, scratch_allocator(&sc)), scratch_allocator(&sc));
    EXPECT_EQ(result.len, 5);
    for (size_t i = 0; i < result.len; i++)
        EXPECT_EQ(*(long long *)slice_get(result, i), (long long)i);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, range_step_two)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Slice result = iter_collect(
        iter_range(0, 10, 2, scratch_allocator(&sc)), scratch_allocator(&sc));
    /* 0, 2, 4, 6, 8 */
    EXPECT_EQ(result.len, 5);
    EXPECT_EQ(*(long long *)slice_get(result, 0), 0LL);
    EXPECT_EQ(*(long long *)slice_get(result, 4), 8LL);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, range_negative_step)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    Slice result = iter_collect(
        iter_range(5, 0, -1, scratch_allocator(&sc)), scratch_allocator(&sc));
    /* 5, 4, 3, 2, 1 */
    EXPECT_EQ(result.len, 5);
    EXPECT_EQ(*(long long *)slice_get(result, 0), 5LL);
    EXPECT_EQ(*(long long *)slice_get(result, 4), 1LL);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, range_empty_when_start_equals_end)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_range(3, 3, 1, scratch_allocator(&sc)));
    EXPECT_EQ(n, 0);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, range_empty_wrong_direction)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    /* positive step but start > end */
    size_t n = iter_count(iter_range(5, 0, 1, scratch_allocator(&sc)));
    EXPECT_EQ(n, 0);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

TEST(iter, range_zero_step_returns_empty)
{
    growing_arena_t _arena_storage;
    growing_arena_t *arena = &_arena_storage;
    growing_arena_init(arena, 64);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, arena);
    size_t n = iter_count(iter_range(0, 10, 0, scratch_allocator(&sc)));
    EXPECT_EQ(n, 0);
    scratch_end(&sc);
    growing_arena_destroy(arena);
}

/* ---- iter_peekable ----------------------------------------------------- */

TEST(iter, peekable_peek_does_not_consume)
{
    int data[] = {10, 20, 30};
    Slice s = {data, 3, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    Iter it = iter_peekable(iter_from_slice(s, scratch_allocator(&sc)));
    int peeked, got;
    EXPECT_TRUE(iter_peek(&it, &peeked));
    EXPECT_EQ(peeked, 10);
    EXPECT_TRUE(it.next(&it, &got));
    EXPECT_EQ(got, 10); /* peek did not consume */
    EXPECT_TRUE(it.next(&it, &got));
    EXPECT_EQ(got, 20);
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, peekable_peek_at_end_returns_false)
{
    int data[] = {1};
    Slice s = {data, 1, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    Iter it = iter_peekable(iter_from_slice(s, scratch_allocator(&sc)));
    int got;
    it.next(&it, &got); /* consume the only element */
    EXPECT_FALSE(iter_peek(&it, &got));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, peekable_multiple_peeks_return_same)
{
    int data[] = {42, 99};
    Slice s = {data, 2, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    Iter it = iter_peekable(iter_from_slice(s, scratch_allocator(&sc)));
    int p1, p2;
    EXPECT_TRUE(iter_peek(&it, &p1));
    EXPECT_TRUE(iter_peek(&it, &p2));
    EXPECT_EQ(p1, p2); /* repeated peek returns same value */
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* ---- iter_dedup -------------------------------------------------------- */

static int int_cmp_dedup(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

TEST(iter, dedup_removes_consecutive_duplicates)
{
    int data[] = {1, 1, 2, 3, 3, 3, 4};
    Slice s = {data, 7, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    Slice result = iter_collect(
        iter_dedup(iter_from_slice(s, scratch_allocator(&sc)), int_cmp_dedup),
        scratch_allocator(&sc));
    EXPECT_EQ(result.len, 4);
    EXPECT_EQ(*(int *)slice_get(result, 0), 1);
    EXPECT_EQ(*(int *)slice_get(result, 1), 2);
    EXPECT_EQ(*(int *)slice_get(result, 2), 3);
    EXPECT_EQ(*(int *)slice_get(result, 3), 4);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, dedup_non_consecutive_duplicates_kept)
{
    int data[] = {1, 2, 1, 2};
    Slice s = {data, 4, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(
        iter_dedup(iter_from_slice(s, scratch_allocator(&sc)), int_cmp_dedup));
    EXPECT_EQ(n, 4); /* no adjacent duplicates */
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(iter, sort_then_dedup_unique_values)
{
    int data[] = {3, 1, 2, 1, 3, 2};
    Slice s = {data, 6, sizeof(int)};
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    Slice result = iter_collect(
        iter_dedup(
            iter_from_slice(
                iter_sort(
                    iter_from_slice(s, scratch_allocator(&sc)),
                    int_cmp_dedup,
                    scratch_allocator(&sc)),
                scratch_allocator(&sc)),
            int_cmp_dedup),
        scratch_allocator(&sc));
    EXPECT_EQ(result.len, 3);
    EXPECT_EQ(*(int *)slice_get(result, 0), 1);
    EXPECT_EQ(*(int *)slice_get(result, 1), 2);
    EXPECT_EQ(*(int *)slice_get(result, 2), 3);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
