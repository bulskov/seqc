#include "ctt.h"
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "oom_alloc.h"
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
TEST(pqueue_empty_on_create)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    ASSERT_EQ(0, pqueue_len(q));
    ASSERT_TRUE(pqueue_is_empty(q));
    ASSERT_NE(SEQC_OK, pqueue_peek(q, NULL));
    int out;
    ASSERT_NE(SEQC_OK, pqueue_pop(q, &out));
    growing_arena_destroy(a);
}
TEST(pqueue_push_and_peek)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
    {
        pqueue_push(q, &vals[i]);
    }
    ASSERT_EQ(5, pqueue_len(q));
    int peeked;
    ASSERT_EQ(SEQC_OK, pqueue_peek(q, &peeked));
    ASSERT_EQ(1, peeked); /* min always at front */
    growing_arena_destroy(a);
}
TEST(pqueue_pop_yields_ascending_order)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6};
    for (int i = 0; i < 9; i++)
    {
        pqueue_push(q, &vals[i]);
    }
    int prev, cur;
    ASSERT_EQ(SEQC_OK, pqueue_pop(q, &prev));
    ASSERT_EQ(1, prev);
    for (int i = 1; i < 9; i++)
    {
        ASSERT_EQ(SEQC_OK, pqueue_pop(q, &cur));
        ASSERT_LE(prev, cur);
        prev = cur;
    }
    ASSERT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
TEST(pqueue_max_heap_via_reverse_cmp)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp_rev, growing_arena_allocator(a));
    int vals[] = {3, 1, 9, 5, 7};
    for (int i = 0; i < 5; i++)
    {
        pqueue_push(q, &vals[i]);
    }
    int peeked;
    ASSERT_EQ(SEQC_OK, pqueue_peek(q, &peeked));
    ASSERT_EQ(9, peeked); /* max at front */
    int prev, cur;
    pqueue_pop(q, &prev);
    while (pqueue_pop(q, &cur) == SEQC_OK)
    {
        ASSERT_GE(prev, cur);
        prev = cur;
    }
    growing_arena_destroy(a);
}
TEST(pqueue_pop_discard_null_out)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {3, 1, 2};
    for (int i = 0; i < 3; i++)
    {
        pqueue_push(q, &vals[i]);
    }
    ASSERT_EQ(SEQC_OK, pqueue_pop(q, NULL)); /* discard min without crash */
    ASSERT_EQ(2, pqueue_len(q));
    int peeked;
    ASSERT_EQ(SEQC_OK, pqueue_peek(q, &peeked));
    ASSERT_EQ(2, peeked);
    growing_arena_destroy(a);
}
TEST(pqueue_push_duplicates)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 5;
    pqueue_push(q, &v);
    pqueue_push(q, &v);
    pqueue_push(q, &v);
    ASSERT_EQ(3, pqueue_len(q));
    int out;
    pqueue_pop(q, &out);
    ASSERT_EQ(5, out);
    pqueue_pop(q, &out);
    ASSERT_EQ(5, out);
    pqueue_pop(q, &out);
    ASSERT_EQ(5, out);
    ASSERT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
TEST(pqueue_interleaved_push_pop)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int v, out;
    v = 5;
    pqueue_push(q, &v);
    v = 3;
    pqueue_push(q, &v);
    ASSERT_EQ(SEQC_OK, pqueue_pop(q, &out));
    ASSERT_EQ(3, out);
    v = 1;
    pqueue_push(q, &v);
    v = 4;
    pqueue_push(q, &v);
    ASSERT_EQ(SEQC_OK, pqueue_pop(q, &out));
    ASSERT_EQ(1, out);
    ASSERT_EQ(SEQC_OK, pqueue_pop(q, &out));
    ASSERT_EQ(4, out);
    ASSERT_EQ(SEQC_OK, pqueue_pop(q, &out));
    ASSERT_EQ(5, out);
    ASSERT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
/* ---- pqueue_clear ------------------------------------------------------ */
TEST(pqueue_clear_empties_queue)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 5; i >= 1; i--)
    {
        pqueue_push(q, &i);
    }
    pqueue_clear(q);
    ASSERT_TRUE(pqueue_is_empty(q));
    ASSERT_EQ(0, pqueue_len(q));
    growing_arena_destroy(a);
}
TEST(pqueue_clear_allows_reuse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 3, 7};
    for (int i = 0; i < 3; i++)
    {
        pqueue_push(q, &vals[i]);
    }
    pqueue_clear(q);
    int x = 42;
    pqueue_push(q, &x);
    int out;
    ASSERT_EQ(SEQC_OK, pqueue_pop(q, &out));
    ASSERT_EQ(42, out);
    ASSERT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
/* ---- pqueue_iter ------------------------------------------------------- */
TEST(pqueue_iter_visits_all_elements)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 1, 3, 2, 4};
    for (int i = 0; i < 5; i++)
    {
        pqueue_push(q, &vals[i]);
    }
    /* Collect all elements via iter (order is heap-storage, not priority) */
    int seen[5];
    size_t n = 0;
    iter_t it = pqueue_iter(q);
    while (it.next(&it, &seen[n]))
    {
        n++;
    }
    iter_drop(&it);
    ASSERT_EQ(5, n);
    /* Verify all 5 values are present, regardless of order */
    int found[5] = {0};
    for (size_t i = 0; i < 5; i++)
    {
        for (int v = 1; v <= 5; v++)
        {
            if (seen[i] == v)
            {
                found[v - 1] = 1;
                break;
            }
        }
    }
    for (int i = 0; i < 5; i++)
    {
        ASSERT_TRUE(found[i]);
    }
    /* queue is unchanged */
    ASSERT_EQ(5, pqueue_len(q));
    growing_arena_destroy(a);
}
TEST(pqueue_iter_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    iter_t it = pqueue_iter(q);
    int v;
    ASSERT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- pqueue_iter_rev --------------------------------------------------- */
TEST(pqueue_iter_rev_visits_all_elements)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {5, 1, 3, 2, 4};
    for (int i = 0; i < 5; i++)
    {
        pqueue_push(q, &vals[i]);
    }
    int seen_fwd[5], seen_rev[5];
    size_t nf = 0, nr = 0;
    iter_t fwd = pqueue_iter(q);
    while (fwd.next(&fwd, &seen_fwd[nf]))
    {
        nf++;
    }
    iter_drop(&fwd);
    iter_t rev = pqueue_iter_rev(q);
    while (rev.next(&rev, &seen_rev[nr]))
    {
        nr++;
    }
    iter_drop(&rev);
    ASSERT_EQ(5, nf);
    ASSERT_EQ(5, nr);
    /* rev must be exactly the reverse of fwd */
    for (size_t i = 0; i < 5; i++)
    {
        ASSERT_EQ(seen_fwd[4 - i], seen_rev[i]);
    }
    growing_arena_destroy(a);
}
TEST(pqueue_iter_rev_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    iter_t it = pqueue_iter_rev(q);
    int v;
    ASSERT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- pqueue_build_from_vec --------------------------------------------- */
TEST(pqueue_build_from_vec_pop_yields_ascending)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6, 0};
    for (int i = 0; i < 10; i++)
    {
        vec_push(v, &vals[i]);
    }
    pqueue_t *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    ASSERT_EQ(10, pqueue_len(q));
    int peeked;
    ASSERT_EQ(SEQC_OK, pqueue_peek(q, &peeked));
    ASSERT_EQ(0, peeked);
    int prev, cur;
    pqueue_pop(q, &prev);
    while (pqueue_pop(q, &cur) == SEQC_OK)
    {
        ASSERT_LE(prev, cur);
        prev = cur;
    }
    ASSERT_TRUE(pqueue_is_empty(q));
    growing_arena_destroy(a);
}
TEST(pqueue_build_from_vec_does_not_modify_source)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {3, 1, 2};
    for (int i = 0; i < 3; i++)
    {
        vec_push(v, &vals[i]);
    }
    pqueue_t *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    /* Original vec must be unchanged */
    ASSERT_EQ(3, vec_len(v));
    ASSERT_EQ(3, *(int *)vec_get(v, 0));
    ASSERT_EQ(1, *(int *)vec_get(v, 1));
    ASSERT_EQ(2, *(int *)vec_get(v, 2));
    (void)q;
    growing_arena_destroy(a);
}
TEST(pqueue_build_from_vec_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    pqueue_t *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    ASSERT_TRUE(pqueue_is_empty(q));
    ASSERT_NE(SEQC_OK, pqueue_peek(q, NULL));
    growing_arena_destroy(a);
}
TEST(pqueue_build_from_vec_single)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    vec_t *v = vec_create(sizeof(int), growing_arena_allocator(a));
    int x = 42;
    vec_push(v, &x);
    pqueue_t *q = pqueue_build_from_vec(v, int_cmp, growing_arena_allocator(a));
    ASSERT_EQ(1, pqueue_len(q));
    int peeked;
    ASSERT_EQ(SEQC_OK, pqueue_peek(q, &peeked));
    ASSERT_EQ(42, peeked);
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises pqueue_free ------------------------------ */
TEST(pqueue_sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    pqueue_t *q = pqueue_create(sizeof(int), int_cmp, al);
    for (int i = 5; i >= 1; i--)
    {
        pqueue_push(q, &i);
    }
    ASSERT_EQ(5, pqueue_len(q));
    int peeked;
    ASSERT_EQ(SEQC_OK, pqueue_peek(q, &peeked));
    ASSERT_EQ(1, peeked);
    pqueue_free(q);
    /* memory released — verified by sys_allocator not leaking */
}
/* ---- pqueue_drain ------------------------------------------------------ */
TEST(pqueue_drain_yields_sorted_slice)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int vals[] = {9, 3, 7, 1, 5, 2, 8, 4, 6};
    for (int i = 0; i < 9; i++)
    {
        pqueue_push(q, &vals[i]);
    }
    slice_t s = pqueue_drain(q, growing_arena_allocator(a));
    ASSERT_EQ(9, s.len);
    ASSERT_TRUE(pqueue_is_empty(q));
    for (size_t i = 1; i < s.len; i++)
    {
        ASSERT_LE(*(int *)slice_get(s, i - 1), *(int *)slice_get(s, i));
    }
    growing_arena_destroy(a);
}
TEST(pqueue_drain_empty_returns_empty_slice)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    slice_t s = pqueue_drain(q, growing_arena_allocator(a));
    ASSERT_EQ(0, s.len);
    ASSERT_NULL(s.ptr);
    growing_arena_destroy(a);
}
TEST(pqueue_drain_queue_is_reusable)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    pqueue_t *q =
        pqueue_create(sizeof(int), int_cmp, growing_arena_allocator(a));
    int x = 7;
    pqueue_push(q, &x);
    pqueue_drain(q, growing_arena_allocator(a));
    ASSERT_TRUE(pqueue_is_empty(q));
    x = 3;
    pqueue_push(q, &x);
    ASSERT_EQ(1, pqueue_len(q));
    growing_arena_destroy(a);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "pqueue_test");
}
