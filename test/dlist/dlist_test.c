#include "ctt.h"

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/dlist.h"

#include "../oom_alloc.h"

/* ---- tests ------------------------------------------------------------- */

TEST(dlist_is_empty_on_create)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_TRUE(dlist_is_empty(l));
    ASSERT_EQ(0, dlist_len(l));
    growing_arena_destroy(a);
}

TEST(dlist_push_back_iter_forward)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
    {
        dlist_push_back(l, &vals[i]);
    }
    iter_t it = dlist_iter(l);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
    {
        n++;
    }
    iter_destroy(&it);
    ASSERT_EQ(3, n);
    ASSERT_EQ(1, got[0]);
    ASSERT_EQ(2, got[1]);
    ASSERT_EQ(3, got[2]);
    growing_arena_destroy(a);
}

TEST(dlist_iter_rev_yields_reverse_order)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
    {
        dlist_push_back(l, &vals[i]);
    }
    iter_t it = dlist_iter_rev(l);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
    {
        n++;
    }
    iter_destroy(&it);
    ASSERT_EQ(3, n);
    ASSERT_EQ(3, got[0]);
    ASSERT_EQ(2, got[1]);
    ASSERT_EQ(1, got[2]);
    growing_arena_destroy(a);
}

TEST(dlist_push_front_prepends)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int two = 2, one = 1;
    dlist_push_back(l, &two);
    dlist_push_front(l, &one);
    ASSERT_EQ(1, *(int *)dlist_front_ptr(l));
    ASSERT_EQ(2, *(int *)dlist_back_ptr(l));
    growing_arena_destroy(a);
}

TEST(dlist_pop_front_removes_head)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
    {
        dlist_push_back(l, &vals[i]);
    }
    int out;
    ASSERT_EQ(SEQC_OK, dlist_pop_front(l, &out));
    ASSERT_EQ(10, out);
    ASSERT_EQ(20, *(int *)dlist_front_ptr(l));
    ASSERT_EQ(2, dlist_len(l));
    growing_arena_destroy(a);
}

TEST(dlist_pop_back_removes_tail)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
    {
        dlist_push_back(l, &vals[i]);
    }
    int out;
    ASSERT_EQ(SEQC_OK, dlist_pop_back(l, &out));
    ASSERT_EQ(30, out);
    ASSERT_EQ(20, *(int *)dlist_back_ptr(l));
    ASSERT_EQ(2, dlist_len(l));
    growing_arena_destroy(a);
}

TEST(dlist_pop_until_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int v = 1;
    dlist_push_back(l, &v);
    int out;
    ASSERT_EQ(SEQC_OK, dlist_pop_front(l, &out));
    ASSERT_NE(SEQC_OK, dlist_pop_front(l, &out));
    ASSERT_TRUE(dlist_is_empty(l));
    ASSERT_NULL(dlist_front_ptr(l));
    ASSERT_NULL(dlist_back_ptr(l));
    growing_arena_destroy(a);
}

TEST(dlist_front_back_null_if_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_NULL(dlist_front_ptr(l));
    ASSERT_NULL(dlist_back_ptr(l));
    growing_arena_destroy(a);
}

TEST(dlist_single_element_front_equals_back)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    dlist_push_back(l, &v);
    ASSERT_EQ(42, *(int *)dlist_front_ptr(l));
    ASSERT_EQ(42, *(int *)dlist_back_ptr(l));
    growing_arena_destroy(a);
}

TEST(dlist_prev_links_are_correct)
{
    /* verify backward linkage by popping from the back repeatedly */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
    {
        dlist_push_back(l, &vals[i]);
    }
    for (int i = 4; i >= 0; i--)
    {
        int out;
        ASSERT_EQ(SEQC_OK, dlist_pop_back(l, &out));
        ASSERT_EQ(vals[i], out);
    }
    ASSERT_TRUE(dlist_is_empty(l));
    growing_arena_destroy(a);
}

TEST(dlist_free_does_not_crash)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int v = 1;
    dlist_push_back(l, &v);
    dlist_push_back(l, &v);
    dlist_destroy(l);
    ASSERT_TRUE(dlist_is_empty(l));
    growing_arena_destroy(a);
}

/* ---- dlist_clear ------------------------------------------------------- */

TEST(dlist_clear_empties_list)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
    {
        dlist_push_back(l, &i);
    }
    dlist_clear(l);
    ASSERT_TRUE(dlist_is_empty(l));
    ASSERT_EQ(0, dlist_len(l));
    growing_arena_destroy(a);
}

TEST(dlist_clear_allows_reuse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    dlist_t *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        dlist_push_back(l, &i);
    }
    dlist_clear(l);
    int x = 99;
    dlist_push_back(l, &x);
    ASSERT_EQ(1, dlist_len(l));
    ASSERT_EQ(99, *(int *)dlist_front_ptr(l));
    growing_arena_destroy(a);
}

/* ---- OOM paths: an exhausted allocator must not crash ------------------ */

TEST(dlist_iter_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    dlist_t *l = dlist_create(sizeof(int), al);
    ASSERT_NOT_NULL(l);
    for (int i = 0; i < 3; i++)
    {
        dlist_push_back(l, &i);
    }
    ctx.remaining = 0; /* exhaust: the iterator's state alloc must fail */
    iter_t it = dlist_iter(l);
    ASSERT_NULL(it.next); /* empty iterator, not a NULL deref */
    iter_destroy(&it);
    dlist_destroy(l);
}

TEST(dlist_iter_rev_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    dlist_t *l = dlist_create(sizeof(int), al);
    ASSERT_NOT_NULL(l);
    for (int i = 0; i < 3; i++)
    {
        dlist_push_back(l, &i);
    }
    ctx.remaining = 0;
    iter_t it = dlist_iter_rev(l);
    ASSERT_NULL(it.next);
    iter_destroy(&it);
    dlist_destroy(l);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "dlist_test");
}
