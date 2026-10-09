#include "ctt.h"

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/ringbuf.h"

/* ---- basic lifecycle --------------------------------------------------- */

TEST(ringbuf_create_is_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_TRUE(ringbuf_is_empty(r));
    ASSERT_EQ(0, ringbuf_len(r));
    growing_arena_destroy(a);
}

/* ---- push_back / pop_front (FIFO) ------------------------------------- */

TEST(ringbuf_push_back_pop_front_fifo_order)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        ASSERT_EQ(SEQC_OK, ringbuf_push_back(r, &i));
    }
    ASSERT_EQ(5, ringbuf_len(r));
    for (int i = 0; i < 5; i++)
    {
        int out;
        ASSERT_EQ(SEQC_OK, ringbuf_pop_front(r, &out));
        ASSERT_EQ(i, out);
    }
    ASSERT_TRUE(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- push_front / pop_back (LIFO from back) --------------------------- */

TEST(ringbuf_push_front_pop_back_order)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    /* push 0,1,2 to front => logical order [2,1,0] */
    for (int i = 0; i < 3; i++)
    {
        ringbuf_push_front(r, &i);
    }
    int out;
    ASSERT_EQ(SEQC_OK, ringbuf_pop_back(r, &out));
    ASSERT_EQ(0, out);
    ASSERT_EQ(SEQC_OK, ringbuf_pop_back(r, &out));
    ASSERT_EQ(1, out);
    ASSERT_EQ(SEQC_OK, ringbuf_pop_back(r, &out));
    ASSERT_EQ(2, out);
    ASSERT_TRUE(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- deque: interleaved push/pop from both ends ----------------------- */

TEST(ringbuf_deque_interleaved)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    int v;

    /* push 10 to back, 20 to front => [20, 10] */
    int ten = 10, twenty = 20;
    ringbuf_push_back(r, &ten);
    ringbuf_push_front(r, &twenty);
    ASSERT_EQ(2, ringbuf_len(r));

    ASSERT_EQ(SEQC_OK, ringbuf_pop_front(r, &v));
    ASSERT_EQ(20, v);
    ASSERT_EQ(SEQC_OK, ringbuf_pop_front(r, &v));
    ASSERT_EQ(10, v);
    growing_arena_destroy(a);
}

/* ---- pop from empty ---------------------------------------------------- */

TEST(ringbuf_pop_empty_returns_not_found)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    int out;
    ASSERT_NE(SEQC_OK, ringbuf_pop_front(r, &out));
    ASSERT_NE(SEQC_OK, ringbuf_pop_back(r, &out));
    growing_arena_destroy(a);
}

/* ---- pop discards with null out --------------------------------------- */

TEST(ringbuf_pop_null_out_discards)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    int v = 7;
    ringbuf_push_back(r, &v);
    ASSERT_EQ(SEQC_OK, ringbuf_pop_front(r, NULL));
    ASSERT_TRUE(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- ringbuf_get -------------------------------------------------------- */

TEST(ringbuf_at_returns_correct_element)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        ringbuf_push_back(r, &i);
    }
    for (int i = 0; i < 5; i++)
    {
        int got;
        ASSERT_EQ(SEQC_OK, ringbuf_get(r, (size_t)i, &got));
        ASSERT_EQ(i, got);
    }
    growing_arena_destroy(a);
}

TEST(ringbuf_at_out_of_bounds_returns_null)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_EQ(SEQC_NOT_FOUND, ringbuf_get(r, 0, NULL));
    growing_arena_destroy(a);
}

/* ---- wrap-around: fill past capacity to trigger grow + wrap ----------- */

TEST(ringbuf_wrap_around_correctness)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    /* Push 20 elements, pop 10 from front, push 10 more
     * to force the head to wrap around the internal buffer */
    for (int i = 0; i < 20; i++)
    {
        ringbuf_push_back(r, &i);
    }
    for (int i = 0; i < 10; i++)
    {
        int out;
        ringbuf_pop_front(r, &out);
        ASSERT_EQ(i, out);
    }
    for (int i = 20; i < 30; i++)
    {
        ringbuf_push_back(r, &i);
    }
    ASSERT_EQ(20, ringbuf_len(r));
    for (int i = 10; i < 30; i++)
    {
        int out;
        ASSERT_EQ(SEQC_OK, ringbuf_pop_front(r, &out));
        ASSERT_EQ(i, out);
    }
    ASSERT_TRUE(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- clear ------------------------------------------------------------- */

TEST(ringbuf_clear_allows_reuse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        ringbuf_push_back(r, &i);
    }
    ringbuf_clear(r);
    ASSERT_TRUE(ringbuf_is_empty(r));
    int v = 99;
    ringbuf_push_back(r, &v);
    ASSERT_EQ(1, ringbuf_len(r));
    int got99;
    ASSERT_EQ(SEQC_OK, ringbuf_get(r, 0, &got99));
    ASSERT_EQ(99, got99);
    growing_arena_destroy(a);
}

/* ---- iter front-to-back ----------------------------------------------- */

TEST(ringbuf_iter_forward)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        ringbuf_push_back(r, &i);
    }
    iter_t it = ringbuf_iter(r);
    int val, expected = 0;
    while (it.next(&it, &val))
    {
        ASSERT_EQ(expected, val);
        expected++;
    }
    iter_destroy(&it);
    ASSERT_EQ(5, expected);
    growing_arena_destroy(a);
}

/* ---- iter back-to-front ----------------------------------------------- */

TEST(ringbuf_iter_reverse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        ringbuf_push_back(r, &i);
    }
    iter_t it = ringbuf_iter_rev(r);
    int val, expected = 4;
    while (it.next(&it, &val))
    {
        ASSERT_EQ(expected, val);
        expected--;
    }
    iter_destroy(&it);
    ASSERT_EQ(-1, expected);
    growing_arena_destroy(a);
}

/* ---- push_front wrap: push elements that cause head to wrap ----------- */

TEST(ringbuf_push_front_wrap)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    /* Push 0..4 to back then prepend 5..9 in reverse order to front.
     * push_front(9), push_front(8), ..., push_front(5)
     * => logical order: [5,6,7,8,9,0,1,2,3,4] */
    for (int i = 0; i < 5; i++)
    {
        ringbuf_push_back(r, &i); /* [0,1,2,3,4] */
    }
    for (int i = 9; i >= 5; i--)
    {
        ringbuf_push_front(
            r, &i); /* prepend 9,8,7,6,5 → [5,6,7,8,9,0,1,2,3,4] */
    }
    ASSERT_EQ(10, ringbuf_len(r));
    int expected[] = {5, 6, 7, 8, 9, 0, 1, 2, 3, 4};
    for (int i = 0; i < 10; i++)
    {
        int got;
        ASSERT_EQ(SEQC_OK, ringbuf_get(r, (size_t)i, &got));
        ASSERT_EQ(expected[i], got);
    }
    growing_arena_destroy(a);
}

/* ---- iter on empty ringbuf --------------------------------------------- */

TEST(ringbuf_iter_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    ringbuf_t *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    iter_t it = ringbuf_iter(r);
    int v;
    ASSERT_FALSE(it.next(&it, &v));
    iter_destroy(&it);
    growing_arena_destroy(a);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "ringbuf_test");
}
