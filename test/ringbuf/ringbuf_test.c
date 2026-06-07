#include <gtest/gtest.h>

extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/ringbuf.h"
}

/* ---- basic lifecycle --------------------------------------------------- */

TEST(ringbuf, create_is_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_TRUE(ringbuf_is_empty(r));
    EXPECT_EQ(ringbuf_len(r), 0);
    growing_arena_destroy(a);
}

/* ---- push_back / pop_front (FIFO) ------------------------------------- */

TEST(ringbuf, push_back_pop_front_fifo_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        EXPECT_EQ(ringbuf_push_back(r, &i), SEQC_OK);
    EXPECT_EQ(ringbuf_len(r), 5);
    for (int i = 0; i < 5; i++)
    {
        int out;
        EXPECT_EQ(ringbuf_pop_front(r, &out), SEQC_OK);
        EXPECT_EQ(out, i);
    }
    EXPECT_TRUE(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- push_front / pop_back (LIFO from back) --------------------------- */

TEST(ringbuf, push_front_pop_back_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    /* push 0,1,2 to front => logical order [2,1,0] */
    for (int i = 0; i < 3; i++)
        ringbuf_push_front(r, &i);
    int out;
    EXPECT_EQ(ringbuf_pop_back(r, &out), SEQC_OK);
    EXPECT_EQ(out, 0);
    EXPECT_EQ(ringbuf_pop_back(r, &out), SEQC_OK);
    EXPECT_EQ(out, 1);
    EXPECT_EQ(ringbuf_pop_back(r, &out), SEQC_OK);
    EXPECT_EQ(out, 2);
    EXPECT_TRUE(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- deque: interleaved push/pop from both ends ----------------------- */

TEST(ringbuf, deque_interleaved)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    int v;

    /* push 10 to back, 20 to front => [20, 10] */
    int ten = 10, twenty = 20;
    ringbuf_push_back(r, &ten);
    ringbuf_push_front(r, &twenty);
    EXPECT_EQ(ringbuf_len(r), 2);

    EXPECT_EQ(ringbuf_pop_front(r, &v), SEQC_OK);
    EXPECT_EQ(v, 20);
    EXPECT_EQ(ringbuf_pop_front(r, &v), SEQC_OK);
    EXPECT_EQ(v, 10);
    growing_arena_destroy(a);
}

/* ---- pop from empty ---------------------------------------------------- */

TEST(ringbuf, pop_empty_returns_not_found)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    int out;
    EXPECT_NE(ringbuf_pop_front(r, &out), SEQC_OK);
    EXPECT_NE(ringbuf_pop_back(r, &out), SEQC_OK);
    growing_arena_destroy(a);
}

/* ---- pop discards with null out --------------------------------------- */

TEST(ringbuf, pop_null_out_discards)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    int v = 7;
    ringbuf_push_back(r, &v);
    EXPECT_EQ(ringbuf_pop_front(r, NULL), SEQC_OK);
    EXPECT_TRUE(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- ringbuf_at -------------------------------------------------------- */

TEST(ringbuf, at_returns_correct_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i);
    for (int i = 0; i < 5; i++) {
        int got;
        EXPECT_EQ(ringbuf_at(r, (size_t)i, &got), SEQC_OK);
        EXPECT_EQ(got, i);
    }
    growing_arena_destroy(a);
}

TEST(ringbuf, at_out_of_bounds_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_EQ(ringbuf_at(r, 0, NULL), SEQC_NOT_FOUND);
    growing_arena_destroy(a);
}

/* ---- wrap-around: fill past capacity to trigger grow + wrap ----------- */

TEST(ringbuf, wrap_around_correctness)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    /* Push 20 elements, pop 10 from front, push 10 more
     * to force the head to wrap around the internal buffer */
    for (int i = 0; i < 20; i++)
        ringbuf_push_back(r, &i);
    for (int i = 0; i < 10; i++)
    {
        int out;
        ringbuf_pop_front(r, &out);
        EXPECT_EQ(out, i);
    }
    for (int i = 20; i < 30; i++)
        ringbuf_push_back(r, &i);
    EXPECT_EQ(ringbuf_len(r), 20);
    for (int i = 10; i < 30; i++)
    {
        int out;
        EXPECT_EQ(ringbuf_pop_front(r, &out), SEQC_OK);
        EXPECT_EQ(out, i);
    }
    EXPECT_TRUE(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- clear ------------------------------------------------------------- */

TEST(ringbuf, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i);
    ringbuf_clear(r);
    EXPECT_TRUE(ringbuf_is_empty(r));
    int v = 99;
    ringbuf_push_back(r, &v);
    EXPECT_EQ(ringbuf_len(r), 1);
    int got99;
    EXPECT_EQ(ringbuf_at(r, 0, &got99), SEQC_OK);
    EXPECT_EQ(got99, 99);
    growing_arena_destroy(a);
}

/* ---- iter front-to-back ----------------------------------------------- */

TEST(ringbuf, iter_forward)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i);
    iter_t it = ringbuf_iter(r);
    int val, expected = 0;
    while (it.next(&it, &val))
    {
        EXPECT_EQ(val, expected);
        expected++;
    }
    iter_drop(&it);
    EXPECT_EQ(expected, 5);
    growing_arena_destroy(a);
}

/* ---- iter back-to-front ----------------------------------------------- */

TEST(ringbuf, iter_reverse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i);
    iter_t it = ringbuf_iter_rev(r);
    int val, expected = 4;
    while (it.next(&it, &val))
    {
        EXPECT_EQ(val, expected);
        expected--;
    }
    iter_drop(&it);
    EXPECT_EQ(expected, -1);
    growing_arena_destroy(a);
}

/* ---- push_front wrap: push elements that cause head to wrap ----------- */

TEST(ringbuf, push_front_wrap)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    /* Push 0..4 to back then prepend 5..9 in reverse order to front.
     * push_front(9), push_front(8), ..., push_front(5)
     * => logical order: [5,6,7,8,9,0,1,2,3,4] */
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i); /* [0,1,2,3,4] */
    for (int i = 9; i >= 5; i--)
        ringbuf_push_front(r, &i); /* prepend 9,8,7,6,5 → [5,6,7,8,9,0,1,2,3,4] */
    EXPECT_EQ(ringbuf_len(r), 10);
    int expected[] = {5, 6, 7, 8, 9, 0, 1, 2, 3, 4};
    for (int i = 0; i < 10; i++) {
        int got;
        EXPECT_EQ(ringbuf_at(r, (size_t)i, &got), SEQC_OK);
        EXPECT_EQ(got, expected[i]);
    }
    growing_arena_destroy(a);
}

/* ---- iter on empty ringbuf --------------------------------------------- */

TEST(ringbuf, iter_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    iter_t it = ringbuf_iter(r);
    int v;
    EXPECT_FALSE(it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
