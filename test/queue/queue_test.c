#include <criterion/criterion.h>
#include "oom_alloc.h"
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/queue.h"
/* ---- tests ------------------------------------------------------------- */
Test(queue, is_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    cr_assert(queue_is_empty(q));
    cr_assert_eq(queue_len(q), 0);
    growing_arena_destroy(a);
}
Test(queue, push_pop_fifo_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    int out;
    cr_assert_eq(queue_pop(q, &out), SEQC_OK);
    cr_assert_eq(out, 1);
    cr_assert_eq(queue_pop(q, &out), SEQC_OK);
    cr_assert_eq(out, 2);
    cr_assert_eq(queue_pop(q, &out), SEQC_OK);
    cr_assert_eq(out, 3);
    cr_assert_neq(queue_pop(q, &out), SEQC_OK);
    growing_arena_destroy(a);
}
Test(queue, peek_does_not_consume)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int v = 99;
    queue_push(q, &v);
    cr_assert_eq(*(int *)queue_peek(q), 99);
    cr_assert_eq(queue_len(q), 1);
    growing_arena_destroy(a);
}
Test(queue, peek_empty_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    cr_assert_null(queue_peek(q));
    growing_arena_destroy(a);
}
Test(queue, pop_empty_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int out;
    cr_assert_neq(queue_pop(q, &out), SEQC_OK);
    growing_arena_destroy(a);
}
Test(queue, ring_wrap_around)
{
    /* Push 16 elements (fills initial cap), pop 8, push 8 more — exercises
     * the ring-buffer wrap and triggers a resize on the 17th push. */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 16; i++)
        queue_push(q, &i);
    for (int i = 0; i < 8; i++)
    {
        int out;
        queue_pop(q, &out);
        cr_assert_eq(out, i);
    }
    /* now head == 8 inside the ring buffer */
    for (int i = 16; i < 24; i++)
        queue_push(q, &i);
    /* drain: expect 8,9,...,23 */
    for (int i = 8; i < 24; i++)
    {
        int out;
        cr_assert_eq(queue_pop(q, &out), SEQC_OK);
        cr_assert_eq(out, i);
    }
    cr_assert(queue_is_empty(q));
    growing_arena_destroy(a);
}
Test(queue, grow_beyond_initial_cap)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 32; i++)
        queue_push(q, &i);
    cr_assert_eq(queue_len(q), 32);
    for (int i = 0; i < 32; i++)
    {
        int out;
        cr_assert_eq(queue_pop(q, &out), SEQC_OK);
        cr_assert_eq(out, i);
    }
    growing_arena_destroy(a);
}
Test(queue, iter_front_to_back)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = queue_iter(q);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 3);
    cr_assert_eq(got[0], 10);
    cr_assert_eq(got[1], 20);
    cr_assert_eq(got[2], 30);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
/* ---- queue_clear ------------------------------------------------------- */
Test(queue, clear_empties_queue)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        queue_push(q, &i);
    queue_clear(q);
    cr_assert(queue_is_empty(q));
    cr_assert_eq(queue_len(q), 0);
    growing_arena_destroy(a);
}
Test(queue, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        queue_push(q, &i);
    queue_clear(q);
    int x = 42;
    queue_push(q, &x);
    int out;
    cr_assert_eq(queue_pop(q, &out), SEQC_OK);
    cr_assert_eq(out, 42);
    cr_assert(queue_is_empty(q));
    growing_arena_destroy(a);
}
/* ---- queue_iter_rev ---------------------------------------------------- */
Test(queue, iter_rev_back_to_front)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = queue_iter_rev(q);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 3);
    cr_assert_eq(got[0], 30);
    cr_assert_eq(got[1], 20);
    cr_assert_eq(got[2], 10);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
Test(queue, iter_rev_wraps_ring_buffer)
{
    /* Push 5, pop 2 to shift head, then check rev order covers the wrap */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        queue_push(q, &vals[i]);
    int discard;
    queue_pop(q, &discard); /* remove 1 */
    queue_pop(q, &discard); /* remove 2 */
    /* queue is now: 3 4 5 (front→back) */
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = queue_iter_rev(q);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 3);
    cr_assert_eq(got[0], 5);
    cr_assert_eq(got[1], 4);
    cr_assert_eq(got[2], 3);
    scratch_end(&sc);
    growing_arena_destroy(a);
}
Test(queue, iter_rev_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    Iter it = queue_iter_rev(q);
    int v;
    cr_assert(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- queue_back --------------------------------------------------------- */
Test(queue, back_returns_last_pushed)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    cr_assert_eq(*(int *)queue_back(q), 30);
    growing_arena_destroy(a);
}
Test(queue, back_differs_from_front_after_pop)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4};
    for (int i = 0; i < 4; i++)
        queue_push(q, &vals[i]);
    int discard;
    queue_pop(q, &discard); /* remove 1 */
    cr_assert_eq(*(int *)queue_peek(q), 2);
    cr_assert_eq(*(int *)queue_back(q), 4);
    growing_arena_destroy(a);
}
/* queue_pop with NULL out: element is consumed but not copied. */
Test(queue, pop_null_out_discards_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    cr_assert_eq(queue_pop(q, NULL), SEQC_OK); /* discard front element */
    cr_assert_eq(queue_len(q), 2);
    cr_assert_eq(*(int *)queue_peek(q), 20); /* 10 is gone */
    growing_arena_destroy(a);
}
/* queue_back on an empty queue must return NULL. */
Test(queue, back_empty_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    Queue *q = queue_create(sizeof(int), growing_arena_allocator(a));
    cr_assert_null(queue_back(q));
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises queue_free ------------------------------- */
Test(queue, sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    Queue *q = queue_create(sizeof(int), al);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        queue_push(q, &vals[i]);
    cr_assert_eq(queue_len(q), 3);
    queue_free(q);
    /* queue_free releases all memory — verified by sys_allocator not leaking */
}
