#include <gtest/gtest.h>
#include "oom_alloc.h"
extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/queue.h"
}
/* ---- tests ------------------------------------------------------------- */
TEST(queue, is_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_TRUE(queue_is_empty(q));
    EXPECT_EQ(queue_len(q), 0);
    growing_arena_destroy(a);
}
TEST(queue, push_pop_fifo_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    int out;
    EXPECT_EQ(queue_pop(q, &out), SEQC_OK);
    EXPECT_EQ(out, 1);
    EXPECT_EQ(queue_pop(q, &out), SEQC_OK);
    EXPECT_EQ(out, 2);
    EXPECT_EQ(queue_pop(q, &out), SEQC_OK);
    EXPECT_EQ(out, 3);
    EXPECT_NE(queue_pop(q, &out), SEQC_OK);
    growing_arena_destroy(a);
}
TEST(queue, peek_does_not_consume)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int v = 99;
    queue_push(q, &v);
    EXPECT_EQ(*(int *)queue_peek(q), 99);
    EXPECT_EQ(queue_len(q), 1);
    growing_arena_destroy(a);
}
TEST(queue, peek_empty_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_EQ(queue_peek(q), nullptr);
    growing_arena_destroy(a);
}
TEST(queue, pop_empty_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int out;
    EXPECT_NE(queue_pop(q, &out), SEQC_OK);
    growing_arena_destroy(a);
}
TEST(queue, ring_wrap_around)
{
    /* Push 16 elements (fills initial cap), pop 8, push 8 more — exercises
     * the ring-buffer wrap and triggers a resize on the 17th push. */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 16; i++)
        queue_push(q, &i);
    for (int i = 0; i < 8; i++)
    {
        int out;
        queue_pop(q, &out);
        EXPECT_EQ(out, i);
    }
    /* now head == 8 inside the ring buffer */
    for (int i = 16; i < 24; i++)
        queue_push(q, &i);
    /* drain: expect 8,9,...,23 */
    for (int i = 8; i < 24; i++)
    {
        int out;
        EXPECT_EQ(queue_pop(q, &out), SEQC_OK);
        EXPECT_EQ(out, i);
    }
    EXPECT_TRUE(queue_is_empty(q));
    growing_arena_destroy(a);
}
TEST(queue, grow_beyond_initial_cap)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 32; i++)
        queue_push(q, &i);
    EXPECT_EQ(queue_len(q), 32);
    for (int i = 0; i < 32; i++)
    {
        int out;
        EXPECT_EQ(queue_pop(q, &out), SEQC_OK);
        EXPECT_EQ(out, i);
    }
    growing_arena_destroy(a);
}
TEST(queue, iter_front_to_back)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = queue_iter(q);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(got[0], 10);
    EXPECT_EQ(got[1], 20);
    EXPECT_EQ(got[2], 30);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
/* ---- queue_clear ------------------------------------------------------- */
TEST(queue, clear_empties_queue)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        queue_push(q, &i);
    queue_clear(q);
    EXPECT_TRUE(queue_is_empty(q));
    EXPECT_EQ(queue_len(q), 0);
    growing_arena_destroy(a);
}
TEST(queue, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        queue_push(q, &i);
    queue_clear(q);
    int x = 42;
    queue_push(q, &x);
    int out;
    EXPECT_EQ(queue_pop(q, &out), SEQC_OK);
    EXPECT_EQ(out, 42);
    EXPECT_TRUE(queue_is_empty(q));
    growing_arena_destroy(a);
}
/* ---- queue_iter_rev ---------------------------------------------------- */
TEST(queue, iter_rev_back_to_front)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = queue_iter_rev(q);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(got[0], 30);
    EXPECT_EQ(got[1], 20);
    EXPECT_EQ(got[2], 10);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
TEST(queue, iter_rev_wraps_ring_buffer)
{
    /* Push 5, pop 2 to shift head, then check rev order covers the wrap */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        queue_push(q, &vals[i]);
    int discard;
    queue_pop(q, &discard); /* remove 1 */
    queue_pop(q, &discard); /* remove 2 */
    /* queue is now: 3 4 5 (front→back) */
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = queue_iter_rev(q);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(got[0], 5);
    EXPECT_EQ(got[1], 4);
    EXPECT_EQ(got[2], 3);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
TEST(queue, iter_rev_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    iter_t it = queue_iter_rev(q);
    int v;
    EXPECT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- queue_back --------------------------------------------------------- */
TEST(queue, back_returns_last_pushed)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    EXPECT_EQ(*(int *)queue_back(q), 30);
    growing_arena_destroy(a);
}
TEST(queue, back_differs_from_front_after_pop)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4};
    for (int i = 0; i < 4; i++)
        queue_push(q, &vals[i]);
    int discard;
    queue_pop(q, &discard); /* remove 1 */
    EXPECT_EQ(*(int *)queue_peek(q), 2);
    EXPECT_EQ(*(int *)queue_back(q), 4);
    growing_arena_destroy(a);
}
/* queue_pop with NULL out: element is consumed but not copied. */
TEST(queue, pop_null_out_discards_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    EXPECT_EQ(queue_pop(q, NULL), SEQC_OK); /* discard front element */
    EXPECT_EQ(queue_len(q), 2);
    EXPECT_EQ(*(int *)queue_peek(q), 20); /* 10 is gone */
    growing_arena_destroy(a);
}
/* queue_back on an empty queue must return NULL. */
TEST(queue, back_empty_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_EQ(queue_back(q), nullptr);
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises queue_free ------------------------------- */
TEST(queue, sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    queue_t *q = queue_create(sizeof(int), al);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    EXPECT_EQ(queue_len(q), 3);
    queue_free(q);
    /* queue_free releases all memory — verified by sys_allocator not leaking */
}

/* ---- OOM paths: an exhausted allocator must not crash ------------------ */

TEST(queue, iter_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    queue_t *q = queue_create(sizeof(int), al);
    ASSERT_NE(q, nullptr);
    for (int i = 0; i < 3; i++)
        queue_push(q, &i);
    ctx.remaining = 0; /* exhaust: the iterator's state alloc must fail */
    iter_t it = queue_iter(q);
    EXPECT_EQ(it.next, nullptr); /* empty iterator, not a NULL deref */
    iter_drop(&it);
    queue_free(q);
}

TEST(queue, iter_rev_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    queue_t *q = queue_create(sizeof(int), al);
    ASSERT_NE(q, nullptr);
    for (int i = 0; i < 3; i++)
        queue_push(q, &i);
    ctx.remaining = 0;
    iter_t it = queue_iter_rev(q);
    EXPECT_EQ(it.next, nullptr);
    iter_drop(&it);
    queue_free(q);
}
