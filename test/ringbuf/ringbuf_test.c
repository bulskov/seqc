#include <criterion/criterion.h>

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/ringbuf.h"

/* ---- basic lifecycle --------------------------------------------------- */

Test(ringbuf, create_is_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    cr_assert(ringbuf_is_empty(r));
    cr_assert_eq(ringbuf_len(r), 0);
    growing_arena_destroy(a);
}

/* ---- push_back / pop_front (FIFO) ------------------------------------- */

Test(ringbuf, push_back_pop_front_fifo_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        cr_assert_eq(ringbuf_push_back(r, &i), SEQC_OK);
    cr_assert_eq(ringbuf_len(r), 5);
    for (int i = 0; i < 5; i++)
    {
        int out;
        cr_assert_eq(ringbuf_pop_front(r, &out), SEQC_OK);
        cr_assert_eq(out, i);
    }
    cr_assert(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- push_front / pop_back (LIFO from back) --------------------------- */

Test(ringbuf, push_front_pop_back_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    /* push 0,1,2 to front => logical order [2,1,0] */
    for (int i = 0; i < 3; i++)
        ringbuf_push_front(r, &i);
    int out;
    cr_assert_eq(ringbuf_pop_back(r, &out), SEQC_OK);
    cr_assert_eq(out, 0);
    cr_assert_eq(ringbuf_pop_back(r, &out), SEQC_OK);
    cr_assert_eq(out, 1);
    cr_assert_eq(ringbuf_pop_back(r, &out), SEQC_OK);
    cr_assert_eq(out, 2);
    cr_assert(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- deque: interleaved push/pop from both ends ----------------------- */

Test(ringbuf, deque_interleaved)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    int v;

    /* push 10 to back, 20 to front => [20, 10] */
    int ten = 10, twenty = 20;
    ringbuf_push_back(r, &ten);
    ringbuf_push_front(r, &twenty);
    cr_assert_eq(ringbuf_len(r), 2);

    cr_assert_eq(ringbuf_pop_front(r, &v), SEQC_OK);
    cr_assert_eq(v, 20);
    cr_assert_eq(ringbuf_pop_front(r, &v), SEQC_OK);
    cr_assert_eq(v, 10);
    growing_arena_destroy(a);
}

/* ---- pop from empty ---------------------------------------------------- */

Test(ringbuf, pop_empty_returns_not_found)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    int out;
    cr_assert_neq(ringbuf_pop_front(r, &out), SEQC_OK);
    cr_assert_neq(ringbuf_pop_back(r, &out), SEQC_OK);
    growing_arena_destroy(a);
}

/* ---- pop discards with null out --------------------------------------- */

Test(ringbuf, pop_null_out_discards)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    int v = 7;
    ringbuf_push_back(r, &v);
    cr_assert_eq(ringbuf_pop_front(r, NULL), SEQC_OK);
    cr_assert(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- ringbuf_at -------------------------------------------------------- */

Test(ringbuf, at_returns_correct_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i);
    for (int i = 0; i < 5; i++) {
        int got;
        cr_assert_eq(ringbuf_at(r, (size_t)i, &got), SEQC_OK);
        cr_assert_eq(got, i);
    }
    growing_arena_destroy(a);
}

Test(ringbuf, at_out_of_bounds_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    cr_assert_eq(ringbuf_at(r, 0, NULL), SEQC_NOT_FOUND);
    growing_arena_destroy(a);
}

/* ---- wrap-around: fill past capacity to trigger grow + wrap ----------- */

Test(ringbuf, wrap_around_correctness)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    /* Push 20 elements, pop 10 from front, push 10 more
     * to force the head to wrap around the internal buffer */
    for (int i = 0; i < 20; i++)
        ringbuf_push_back(r, &i);
    for (int i = 0; i < 10; i++)
    {
        int out;
        ringbuf_pop_front(r, &out);
        cr_assert_eq(out, i);
    }
    for (int i = 20; i < 30; i++)
        ringbuf_push_back(r, &i);
    cr_assert_eq(ringbuf_len(r), 20);
    for (int i = 10; i < 30; i++)
    {
        int out;
        cr_assert_eq(ringbuf_pop_front(r, &out), SEQC_OK);
        cr_assert_eq(out, i);
    }
    cr_assert(ringbuf_is_empty(r));
    growing_arena_destroy(a);
}

/* ---- clear ------------------------------------------------------------- */

Test(ringbuf, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i);
    ringbuf_clear(r);
    cr_assert(ringbuf_is_empty(r));
    int v = 99;
    ringbuf_push_back(r, &v);
    cr_assert_eq(ringbuf_len(r), 1);
    int got99;
    cr_assert_eq(ringbuf_at(r, 0, &got99), SEQC_OK);
    cr_assert_eq(got99, 99);
    growing_arena_destroy(a);
}

/* ---- iter front-to-back ----------------------------------------------- */

Test(ringbuf, iter_forward)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i);
    Iter it = ringbuf_iter(r);
    int val, expected = 0;
    while (it.next(&it, &val))
    {
        cr_assert_eq(val, expected);
        expected++;
    }
    iter_drop(&it);
    cr_assert_eq(expected, 5);
    growing_arena_destroy(a);
}

/* ---- iter back-to-front ----------------------------------------------- */

Test(ringbuf, iter_reverse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i);
    Iter it = ringbuf_iter_rev(r);
    int val, expected = 4;
    while (it.next(&it, &val))
    {
        cr_assert_eq(val, expected);
        expected--;
    }
    iter_drop(&it);
    cr_assert_eq(expected, -1);
    growing_arena_destroy(a);
}

/* ---- push_front wrap: push elements that cause head to wrap ----------- */

Test(ringbuf, push_front_wrap)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    /* Push 0..4 to back then prepend 5..9 in reverse order to front.
     * push_front(9), push_front(8), ..., push_front(5)
     * => logical order: [5,6,7,8,9,0,1,2,3,4] */
    for (int i = 0; i < 5; i++)
        ringbuf_push_back(r, &i); /* [0,1,2,3,4] */
    for (int i = 9; i >= 5; i--)
        ringbuf_push_front(r, &i); /* prepend 9,8,7,6,5 → [5,6,7,8,9,0,1,2,3,4] */
    cr_assert_eq(ringbuf_len(r), 10);
    int expected[] = {5, 6, 7, 8, 9, 0, 1, 2, 3, 4};
    for (int i = 0; i < 10; i++) {
        int got;
        cr_assert_eq(ringbuf_at(r, (size_t)i, &got), SEQC_OK);
        cr_assert_eq(got, expected[i]);
    }
    growing_arena_destroy(a);
}

/* ---- iter on empty ringbuf --------------------------------------------- */

Test(ringbuf, iter_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    RingBuf *r = ringbuf_create(sizeof(int), growing_arena_allocator(a));
    Iter it = ringbuf_iter(r);
    int v;
    cr_assert_not(it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
