#include <criterion/criterion.h>
#include "oom_alloc.h"
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/pqueue.h"
#include "seqc/vec.h"
static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}
/* negated comparator → max-heap */
static int int_cmp_rev(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (y > x) - (y < x);
}
Test(pqueue, empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    cr_assert_eq(pqueue_len(q), 0);
    cr_assert(pqueue_is_empty(q));
    cr_assert_neq(pqueue_peek(q, NULL), SEQC_OK);
    int out;
    cr_assert_neq(pqueue_pop(q, &out), SEQC_OK);
    growing_arena_destroy(a);
}
Test(pqueue, push_and_peek)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
        pqueue_push(q, &vals[i]);
    cr_assert_eq(pqueue_len(q), 5);
    int peeked;
    cr_assert_eq(pqueue_peek(q, &peeked), SEQC_OK);
    cr_assert_eq(peeked, 1); /* min always at front */
    growing_arena_destroy(a);
}
Test(pqueue, pop_yields_ascending_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6};
    for (int i = 0; i < 9; i++)
        pqueue_push(q, &vals[i]);
    int prev, cur;
    cr_assert_eq(pqueue_pop(q, &prev), SEQC_OK);
    cr_assert_eq(prev, 1);
    for (int i = 1; i < 9; i++)
    {
        cr_assert_eq(pqueue_pop(q, &cur), SEQC_OK);
        cr_assert_leq(prev, cur);
        prev = cur;
    }
    cr_assert(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
Test(pqueue, max_heap_via_reverse_cmp)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp_rev, growing_arena_allocator(a));
    int vals[] = {3, 1, 9, 5, 7};
    for (int i = 0; i < 5; i++)
        pqueue_push(q, &vals[i]);
    int peeked;
    cr_assert_eq(pqueue_peek(q, &peeked), SEQC_OK);
    cr_assert_eq(peeked, 9); /* max at front */
    int prev, cur;
    pqueue_pop(q, &prev);
    while (pqueue_pop(q, &cur) == SEQC_OK)
    {
        cr_assert_geq(prev, cur);
        prev = cur;
    }
    growing_arena_destroy(a);
}
Test(pqueue, pop_discard_null_out)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 2};
    for (int i = 0; i < 3; i++)
        pqueue_push(q, &vals[i]);
    cr_assert_eq(pqueue_pop(q, NULL), SEQC_OK); /* discard min without crash */
    cr_assert_eq(pqueue_len(q), 2);
    int peeked;
    cr_assert_eq(pqueue_peek(q, &peeked), SEQC_OK);
    cr_assert_eq(peeked, 2);
    growing_arena_destroy(a);
}
Test(pqueue, push_duplicates)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 5;
    pqueue_push(q, &v);
    pqueue_push(q, &v);
    pqueue_push(q, &v);
    cr_assert_eq(pqueue_len(q), 3);
    int out;
    pqueue_pop(q, &out);
    cr_assert_eq(out, 5);
    pqueue_pop(q, &out);
    cr_assert_eq(out, 5);
    pqueue_pop(q, &out);
    cr_assert_eq(out, 5);
    cr_assert(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
Test(pqueue, interleaved_push_pop)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v, out;
    v = 5;
    pqueue_push(q, &v);
    v = 3;
    pqueue_push(q, &v);
    cr_assert_eq(pqueue_pop(q, &out), SEQC_OK);
    cr_assert_eq(out, 3);
    v = 1;
    pqueue_push(q, &v);
    v = 4;
    pqueue_push(q, &v);
    cr_assert_eq(pqueue_pop(q, &out), SEQC_OK);
    cr_assert_eq(out, 1);
    cr_assert_eq(pqueue_pop(q, &out), SEQC_OK);
    cr_assert_eq(out, 4);
    cr_assert_eq(pqueue_pop(q, &out), SEQC_OK);
    cr_assert_eq(out, 5);
    cr_assert(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
/* ---- pqueue_clear ------------------------------------------------------ */
Test(pqueue, clear_empties_queue)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 5; i >= 1; i--)
        pqueue_push(q, &i);
    pqueue_clear(q);
    cr_assert(pqueue_is_empty(q));
    cr_assert_eq(pqueue_len(q), 0);
    growing_arena_destroy(a);
}
Test(pqueue, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7};
    for (int i = 0; i < 3; i++)
        pqueue_push(q, &vals[i]);
    pqueue_clear(q);
    int x = 42;
    pqueue_push(q, &x);
    int out;
    cr_assert_eq(pqueue_pop(q, &out), SEQC_OK);
    cr_assert_eq(out, 42);
    cr_assert(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
/* ---- pqueue_iter ------------------------------------------------------- */
Test(pqueue, iter_visits_all_elements)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 1, 3, 2, 4};
    for (int i = 0; i < 5; i++)
        pqueue_push(q, &vals[i]);
    /* Collect all elements via iter (order is heap-storage, not priority) */
    int seen[5];
    size_t n = 0;
    Iter it = pqueue_iter(q);
    while (it.next(&it, &seen[n]))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 5);
    /* Verify all 5 values are present, regardless of order */
    int found[5] = {0};
    for (size_t i = 0; i < 5; i++)
        for (int v = 1; v <= 5; v++)
            if (seen[i] == v)
            {
                found[v - 1] = 1;
                break;
            }
    for (int i = 0; i < 5; i++)
        cr_assert(found[i]);
    /* queue is unchanged */
    cr_assert_eq(pqueue_len(q), 5);
    growing_arena_destroy(a);
}
Test(pqueue, iter_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    Iter it = pqueue_iter(q);
    int v;
    cr_assert(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- pqueue_iter_rev --------------------------------------------------- */
Test(pqueue, iter_rev_visits_all_elements)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 1, 3, 2, 4};
    for (int i = 0; i < 5; i++)
        pqueue_push(q, &vals[i]);
    int seen_fwd[5], seen_rev[5];
    size_t nf = 0, nr = 0;
    Iter fwd = pqueue_iter(q);
    while (fwd.next(&fwd, &seen_fwd[nf]))
        nf++;
    iter_drop(&fwd);
    Iter rev = pqueue_iter_rev(q);
    while (rev.next(&rev, &seen_rev[nr]))
        nr++;
    iter_drop(&rev);
    cr_assert_eq(nf, 5);
    cr_assert_eq(nr, 5);
    /* rev must be exactly the reverse of fwd */
    for (size_t i = 0; i < 5; i++)
        cr_assert_eq(seen_rev[i], seen_fwd[4 - i]);
    growing_arena_destroy(a);
}
Test(pqueue, iter_rev_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    Iter it = pqueue_iter_rev(q);
    int v;
    cr_assert(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- pqueue_build_from_vec --------------------------------------------- */
Test(pqueue, build_from_vec_pop_yields_ascending)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6, 0};
    for (int i = 0; i < 10; i++)
        vec_push(v, &vals[i]);
    PQueue *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    cr_assert_eq(pqueue_len(q), 10);
    int peeked;
    cr_assert_eq(pqueue_peek(q, &peeked), SEQC_OK);
    cr_assert_eq(peeked, 0);
    int prev, cur;
    pqueue_pop(q, &prev);
    while (pqueue_pop(q, &cur) == SEQC_OK)
    {
        cr_assert_leq(prev, cur);
        prev = cur;
    }
    cr_assert(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
Test(pqueue, build_from_vec_does_not_modify_source)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {3, 1, 2};
    for (int i = 0; i < 3; i++)
        vec_push(v, &vals[i]);
    PQueue *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    /* Original vec must be unchanged */
    cr_assert_eq(vec_len(v), 3);
    cr_assert_eq(*(int *)vec_get(v, 0), 3);
    cr_assert_eq(*(int *)vec_get(v, 1), 1);
    cr_assert_eq(*(int *)vec_get(v, 2), 2);
    (void)q;
    growing_arena_destroy(a);
}
Test(pqueue, build_from_vec_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    PQueue *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    cr_assert(pqueue_is_empty(q));
    cr_assert_neq(pqueue_peek(q, NULL), SEQC_OK);
    growing_arena_destroy(a);
}
Test(pqueue, build_from_vec_single)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    PQueue *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    cr_assert_eq(pqueue_len(q), 1);
    int peeked;
    cr_assert_eq(pqueue_peek(q, &peeked), SEQC_OK);
    cr_assert_eq(peeked, 42);
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises pqueue_free ------------------------------ */
Test(pqueue, sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    PQueue *q = pqueue_create(sizeof(int), int_cmp, al);
    for (int i = 5; i >= 1; i--)
        pqueue_push(q, &i);
    cr_assert_eq(pqueue_len(q), 5);
    int peeked;
    cr_assert_eq(pqueue_peek(q, &peeked), SEQC_OK);
    cr_assert_eq(peeked, 1);
    pqueue_free(q);
    /* memory released — verified by sys_allocator not leaking */
}
/* ---- pqueue_drain ------------------------------------------------------ */
Test(pqueue, drain_yields_sorted_slice)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6};
    for (int i = 0; i < 9; i++)
        pqueue_push(q, &vals[i]);
    Slice s = pqueue_drain(q, growing_arena_allocator(a));
    cr_assert_eq(s.len, 9);
    cr_assert(pqueue_is_empty(q));
    for (size_t i = 1; i < s.len; i++)
        cr_assert_leq(*(int *)slice_get(s, i - 1), *(int *)slice_get(s, i));
    growing_arena_destroy(a);
}
Test(pqueue, drain_empty_returns_empty_slice)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    Slice s = pqueue_drain(q, growing_arena_allocator(a));
    cr_assert_eq(s.len, 0);
    cr_assert_null(s.ptr);
    growing_arena_destroy(a);
}
Test(pqueue, drain_queue_is_reusable)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int x = 7;
    pqueue_push(q, &x);
    pqueue_drain(q, growing_arena_allocator(a));
    cr_assert(pqueue_is_empty(q));
    x = 3;
    pqueue_push(q, &x);
    cr_assert_eq(pqueue_len(q), 1);
    growing_arena_destroy(a);
}
