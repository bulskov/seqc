#include "ctt.h"
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "oom_alloc.h"
#include "seqc/queue.h"
/* ---- tests ------------------------------------------------------------- */
TEST(queue_is_empty_on_create)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_TRUE(queue_is_empty(q));
    ASSERT_EQ(0, queue_len(q));
    growing_arena_destroy(a);
}
TEST(queue_push_pop_fifo_order)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
    {
        queue_push(q, &vals[i]);
    }
    int out;
    ASSERT_EQ(SEQC_OK, queue_pop(q, &out));
    ASSERT_EQ(1, out);
    ASSERT_EQ(SEQC_OK, queue_pop(q, &out));
    ASSERT_EQ(2, out);
    ASSERT_EQ(SEQC_OK, queue_pop(q, &out));
    ASSERT_EQ(3, out);
    ASSERT_NE(SEQC_OK, queue_pop(q, &out));
    growing_arena_destroy(a);
}
TEST(queue_peek_does_not_consume)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int v = 99;
    queue_push(q, &v);
    ASSERT_EQ(99, *(int *)queue_peek(q));
    ASSERT_EQ(1, queue_len(q));
    growing_arena_destroy(a);
}
TEST(queue_peek_empty_returns_null)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_NULL(queue_peek(q));
    growing_arena_destroy(a);
}
TEST(queue_pop_empty_returns_0)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int out;
    ASSERT_NE(SEQC_OK, queue_pop(q, &out));
    growing_arena_destroy(a);
}
TEST(queue_ring_wrap_around)
{
    /* Push 16 elements (fills initial cap), pop 8, push 8 more — exercises
     * the ring-buffer wrap and triggers a resize on the 17th push. */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 16; i++)
    {
        queue_push(q, &i);
    }
    for (int i = 0; i < 8; i++)
    {
        int out;
        queue_pop(q, &out);
        ASSERT_EQ(i, out);
    }
    /* now head == 8 inside the ring buffer */
    for (int i = 16; i < 24; i++)
    {
        queue_push(q, &i);
    }
    /* drain: expect 8,9,...,23 */
    for (int i = 8; i < 24; i++)
    {
        int out;
        ASSERT_EQ(SEQC_OK, queue_pop(q, &out));
        ASSERT_EQ(i, out);
    }
    ASSERT_TRUE(queue_is_empty(q));
    growing_arena_destroy(a);
}
TEST(queue_grow_beyond_initial_cap)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 32; i++)
    {
        queue_push(q, &i);
    }
    ASSERT_EQ(32, queue_len(q));
    for (int i = 0; i < 32; i++)
    {
        int out;
        ASSERT_EQ(SEQC_OK, queue_pop(q, &out));
        ASSERT_EQ(i, out);
    }
    growing_arena_destroy(a);
}
TEST(queue_iter_front_to_back)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
    {
        queue_push(q, &vals[i]);
    }
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = queue_iter(q);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
    {
        n++;
    }
    iter_drop(&it);
    ASSERT_EQ(3, n);
    ASSERT_EQ(10, got[0]);
    ASSERT_EQ(20, got[1]);
    ASSERT_EQ(30, got[2]);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
/* ---- queue_clear ------------------------------------------------------- */
TEST(queue_clear_empties_queue)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
    {
        queue_push(q, &i);
    }
    queue_clear(q);
    ASSERT_TRUE(queue_is_empty(q));
    ASSERT_EQ(0, queue_len(q));
    growing_arena_destroy(a);
}
TEST(queue_clear_allows_reuse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        queue_push(q, &i);
    }
    queue_clear(q);
    int x = 42;
    queue_push(q, &x);
    int out;
    ASSERT_EQ(SEQC_OK, queue_pop(q, &out));
    ASSERT_EQ(42, out);
    ASSERT_TRUE(queue_is_empty(q));
    growing_arena_destroy(a);
}
/* ---- queue_iter_rev ---------------------------------------------------- */
TEST(queue_iter_rev_back_to_front)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
    {
        queue_push(q, &vals[i]);
    }
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = queue_iter_rev(q);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
    {
        n++;
    }
    iter_drop(&it);
    ASSERT_EQ(3, n);
    ASSERT_EQ(30, got[0]);
    ASSERT_EQ(20, got[1]);
    ASSERT_EQ(10, got[2]);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
TEST(queue_iter_rev_wraps_ring_buffer)
{
    /* Push 5, pop 2 to shift head, then check rev order covers the wrap */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
    {
        queue_push(q, &vals[i]);
    }
    int discard;
    queue_pop(q, &discard); /* remove 1 */
    queue_pop(q, &discard); /* remove 2 */
    /* queue is now: 3 4 5 (front→back) */
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = queue_iter_rev(q);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
    {
        n++;
    }
    iter_drop(&it);
    ASSERT_EQ(3, n);
    ASSERT_EQ(5, got[0]);
    ASSERT_EQ(4, got[1]);
    ASSERT_EQ(3, got[2]);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
TEST(queue_iter_rev_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    iter_t it = queue_iter_rev(q);
    int v;
    ASSERT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- queue_back --------------------------------------------------------- */
TEST(queue_back_returns_last_pushed)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
    {
        queue_push(q, &vals[i]);
    }
    ASSERT_EQ(30, *(int *)queue_back(q));
    growing_arena_destroy(a);
}
TEST(queue_back_differs_from_front_after_pop)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4};
    for (int i = 0; i < 4; i++)
    {
        queue_push(q, &vals[i]);
    }
    int discard;
    queue_pop(q, &discard); /* remove 1 */
    ASSERT_EQ(2, *(int *)queue_peek(q));
    ASSERT_EQ(4, *(int *)queue_back(q));
    growing_arena_destroy(a);
}
/* queue_pop with NULL out: element is consumed but not copied. */
TEST(queue_pop_null_out_discards_element)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
    {
        queue_push(q, &vals[i]);
    }
    ASSERT_EQ(SEQC_OK, queue_pop(q, NULL)); /* discard front element */
    ASSERT_EQ(2, queue_len(q));
    ASSERT_EQ(20, *(int *)queue_peek(q)); /* 10 is gone */
    growing_arena_destroy(a);
}
/* queue_back on an empty queue must return NULL. */
TEST(queue_back_empty_returns_null)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);
    queue_t *q = queue_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_NULL(queue_back(q));
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises queue_free ------------------------------- */
TEST(queue_sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    queue_t *q = queue_create(sizeof(int), al);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
    {
        queue_push(q, &vals[i]);
    }
    ASSERT_EQ(3, queue_len(q));
    queue_free(q);
    /* queue_free releases all memory — verified by sys_allocator not leaking */
}

/* ---- OOM paths: an exhausted allocator must not crash ------------------ */

TEST(queue_iter_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    queue_t *q = queue_create(sizeof(int), al);
    ASSERT_NOT_NULL(q);
    for (int i = 0; i < 3; i++)
    {
        queue_push(q, &i);
    }
    ctx.remaining = 0; /* exhaust: the iterator's state alloc must fail */
    iter_t it = queue_iter(q);
    ASSERT_NULL(it.next); /* empty iterator, not a NULL deref */
    iter_drop(&it);
    queue_free(q);
}

TEST(queue_iter_rev_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    queue_t *q = queue_create(sizeof(int), al);
    ASSERT_NOT_NULL(q);
    for (int i = 0; i < 3; i++)
    {
        queue_push(q, &i);
    }
    ctx.remaining = 0;
    iter_t it = queue_iter_rev(q);
    ASSERT_NULL(it.next);
    iter_drop(&it);
    queue_free(q);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "queue_test");
}
