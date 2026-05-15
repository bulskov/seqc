#include <gtest/gtest.h>
#include "oom_alloc.h"
extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/pqueue.h"
#include "seqc/vec.h"
}
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
TEST(pqueue, empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    EXPECT_EQ(pqueue_len(q), 0);
    EXPECT_TRUE(pqueue_is_empty(q));
    EXPECT_NE(pqueue_peek(q, NULL), SEQC_OK);
    int out;
    EXPECT_NE(pqueue_pop(q, &out), SEQC_OK);
    growing_arena_destroy(a);
}
TEST(pqueue, push_and_peek)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
        pqueue_push(q, &vals[i]);
    EXPECT_EQ(pqueue_len(q), 5);
    int peeked;
    EXPECT_EQ(pqueue_peek(q, &peeked), SEQC_OK);
    EXPECT_EQ(peeked, 1); /* min always at front */
    growing_arena_destroy(a);
}
TEST(pqueue, pop_yields_ascending_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6};
    for (int i = 0; i < 9; i++)
        pqueue_push(q, &vals[i]);
    int prev, cur;
    EXPECT_EQ(pqueue_pop(q, &prev), SEQC_OK);
    EXPECT_EQ(prev, 1);
    for (int i = 1; i < 9; i++)
    {
        EXPECT_EQ(pqueue_pop(q, &cur), SEQC_OK);
        EXPECT_LE(prev, cur);
        prev = cur;
    }
    EXPECT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
TEST(pqueue, max_heap_via_reverse_cmp)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp_rev, growing_arena_allocator(a));
    int vals[] = {3, 1, 9, 5, 7};
    for (int i = 0; i < 5; i++)
        pqueue_push(q, &vals[i]);
    int peeked;
    EXPECT_EQ(pqueue_peek(q, &peeked), SEQC_OK);
    EXPECT_EQ(peeked, 9); /* max at front */
    int prev, cur;
    pqueue_pop(q, &prev);
    while (pqueue_pop(q, &cur) == SEQC_OK)
    {
        EXPECT_GE(prev, cur);
        prev = cur;
    }
    growing_arena_destroy(a);
}
TEST(pqueue, pop_discard_null_out)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 2};
    for (int i = 0; i < 3; i++)
        pqueue_push(q, &vals[i]);
    EXPECT_EQ(pqueue_pop(q, NULL), SEQC_OK); /* discard min without crash */
    EXPECT_EQ(pqueue_len(q), 2);
    int peeked;
    EXPECT_EQ(pqueue_peek(q, &peeked), SEQC_OK);
    EXPECT_EQ(peeked, 2);
    growing_arena_destroy(a);
}
TEST(pqueue, push_duplicates)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 5;
    pqueue_push(q, &v);
    pqueue_push(q, &v);
    pqueue_push(q, &v);
    EXPECT_EQ(pqueue_len(q), 3);
    int out;
    pqueue_pop(q, &out);
    EXPECT_EQ(out, 5);
    pqueue_pop(q, &out);
    EXPECT_EQ(out, 5);
    pqueue_pop(q, &out);
    EXPECT_EQ(out, 5);
    EXPECT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
TEST(pqueue, interleaved_push_pop)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v, out;
    v = 5;
    pqueue_push(q, &v);
    v = 3;
    pqueue_push(q, &v);
    EXPECT_EQ(pqueue_pop(q, &out), SEQC_OK);
    EXPECT_EQ(out, 3);
    v = 1;
    pqueue_push(q, &v);
    v = 4;
    pqueue_push(q, &v);
    EXPECT_EQ(pqueue_pop(q, &out), SEQC_OK);
    EXPECT_EQ(out, 1);
    EXPECT_EQ(pqueue_pop(q, &out), SEQC_OK);
    EXPECT_EQ(out, 4);
    EXPECT_EQ(pqueue_pop(q, &out), SEQC_OK);
    EXPECT_EQ(out, 5);
    EXPECT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
/* ---- pqueue_clear ------------------------------------------------------ */
TEST(pqueue, clear_empties_queue)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 5; i >= 1; i--)
        pqueue_push(q, &i);
    pqueue_clear(q);
    EXPECT_TRUE(pqueue_is_empty(q));
    EXPECT_EQ(pqueue_len(q), 0);
    growing_arena_destroy(a);
}
TEST(pqueue, clear_allows_reuse)
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
    EXPECT_EQ(pqueue_pop(q, &out), SEQC_OK);
    EXPECT_EQ(out, 42);
    EXPECT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
/* ---- pqueue_iter ------------------------------------------------------- */
TEST(pqueue, iter_visits_all_elements)
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
    EXPECT_EQ(n, 5);
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
        EXPECT_TRUE(found[i]);
    /* queue is unchanged */
    EXPECT_EQ(pqueue_len(q), 5);
    growing_arena_destroy(a);
}
TEST(pqueue, iter_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    Iter it = pqueue_iter(q);
    int v;
    EXPECT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- pqueue_iter_rev --------------------------------------------------- */
TEST(pqueue, iter_rev_visits_all_elements)
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
    EXPECT_EQ(nf, 5);
    EXPECT_EQ(nr, 5);
    /* rev must be exactly the reverse of fwd */
    for (size_t i = 0; i < 5; i++)
        EXPECT_EQ(seen_rev[i], seen_fwd[4 - i]);
    growing_arena_destroy(a);
}
TEST(pqueue, iter_rev_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    Iter it = pqueue_iter_rev(q);
    int v;
    EXPECT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- pqueue_build_from_vec --------------------------------------------- */
TEST(pqueue, build_from_vec_pop_yields_ascending)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6, 0};
    for (int i = 0; i < 10; i++)
        vec_push(v, &vals[i]);
    PQueue *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    EXPECT_EQ(pqueue_len(q), 10);
    int peeked;
    EXPECT_EQ(pqueue_peek(q, &peeked), SEQC_OK);
    EXPECT_EQ(peeked, 0);
    int prev, cur;
    pqueue_pop(q, &prev);
    while (pqueue_pop(q, &cur) == SEQC_OK)
    {
        EXPECT_LE(prev, cur);
        prev = cur;
    }
    EXPECT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
TEST(pqueue, build_from_vec_does_not_modify_source)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {3, 1, 2};
    for (int i = 0; i < 3; i++)
        vec_push(v, &vals[i]);
    PQueue *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    /* Original vec must be unchanged */
    EXPECT_EQ(vec_len(v), 3);
    EXPECT_EQ(*(int *)vec_get(v, 0), 3);
    EXPECT_EQ(*(int *)vec_get(v, 1), 1);
    EXPECT_EQ(*(int *)vec_get(v, 2), 2);
    (void)q;
    growing_arena_destroy(a);
}
TEST(pqueue, build_from_vec_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    PQueue *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    EXPECT_TRUE(pqueue_is_empty(q));
    EXPECT_NE(pqueue_peek(q, NULL), SEQC_OK);
    growing_arena_destroy(a);
}
TEST(pqueue, build_from_vec_single)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Vec *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    PQueue *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    EXPECT_EQ(pqueue_len(q), 1);
    int peeked;
    EXPECT_EQ(pqueue_peek(q, &peeked), SEQC_OK);
    EXPECT_EQ(peeked, 42);
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises pqueue_free ------------------------------ */
TEST(pqueue, sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    PQueue *q = pqueue_create(sizeof(int), int_cmp, al);
    for (int i = 5; i >= 1; i--)
        pqueue_push(q, &i);
    EXPECT_EQ(pqueue_len(q), 5);
    int peeked;
    EXPECT_EQ(pqueue_peek(q, &peeked), SEQC_OK);
    EXPECT_EQ(peeked, 1);
    pqueue_free(q);
    /* memory released — verified by sys_allocator not leaking */
}
/* ---- pqueue_drain ------------------------------------------------------ */
TEST(pqueue, drain_yields_sorted_slice)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6};
    for (int i = 0; i < 9; i++)
        pqueue_push(q, &vals[i]);
    Slice s = pqueue_drain(q, growing_arena_allocator(a));
    EXPECT_EQ(s.len, 9);
    EXPECT_TRUE(pqueue_is_empty(q));
    for (size_t i = 1; i < s.len; i++)
        EXPECT_LE(*(int *)slice_get(s, i - 1), *(int *)slice_get(s, i));
    growing_arena_destroy(a);
}
TEST(pqueue, drain_empty_returns_empty_slice)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    Slice s = pqueue_drain(q, growing_arena_allocator(a));
    EXPECT_EQ(s.len, 0);
    EXPECT_EQ(s.ptr, nullptr);
    growing_arena_destroy(a);
}
TEST(pqueue, drain_queue_is_reusable)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    PQueue *q = pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int x = 7;
    pqueue_push(q, &x);
    pqueue_drain(q, growing_arena_allocator(a));
    EXPECT_TRUE(pqueue_is_empty(q));
    x = 3;
    pqueue_push(q, &x);
    EXPECT_EQ(pqueue_len(q), 1);
    growing_arena_destroy(a);
}
