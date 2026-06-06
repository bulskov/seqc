#include <gtest/gtest.h>

extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/dlist.h"
}

#include "../oom_alloc.h"

/* ---- tests ------------------------------------------------------------- */

TEST(dlist, is_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_TRUE(dlist_is_empty(l));
    EXPECT_EQ(dlist_len(l), 0);
    growing_arena_destroy(a);
}

TEST(dlist, push_back_iter_forward)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &vals[i]);
    Iter it = dlist_iter(l);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(got[0], 1);
    EXPECT_EQ(got[1], 2);
    EXPECT_EQ(got[2], 3);
    growing_arena_destroy(a);
}

TEST(dlist, iter_rev)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &vals[i]);
    Iter it = dlist_iter_rev(l);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(got[0], 3);
    EXPECT_EQ(got[1], 2);
    EXPECT_EQ(got[2], 1);
    growing_arena_destroy(a);
}

TEST(dlist, push_front_prepends)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int two = 2, one = 1;
    dlist_push_back(l, &two);
    dlist_push_front(l, &one);
    EXPECT_EQ(*(int *)dlist_front(l), 1);
    EXPECT_EQ(*(int *)dlist_back(l), 2);
    growing_arena_destroy(a);
}

TEST(dlist, pop_front_removes_head)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &vals[i]);
    int out;
    EXPECT_EQ(dlist_pop_front(l, &out), SEQC_OK);
    EXPECT_EQ(out, 10);
    EXPECT_EQ(*(int *)dlist_front(l), 20);
    EXPECT_EQ(dlist_len(l), 2);
    growing_arena_destroy(a);
}

TEST(dlist, pop_back_removes_tail)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &vals[i]);
    int out;
    EXPECT_EQ(dlist_pop_back(l, &out), SEQC_OK);
    EXPECT_EQ(out, 30);
    EXPECT_EQ(*(int *)dlist_back(l), 20);
    EXPECT_EQ(dlist_len(l), 2);
    growing_arena_destroy(a);
}

TEST(dlist, pop_until_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int v = 1;
    dlist_push_back(l, &v);
    int out;
    EXPECT_EQ(dlist_pop_front(l, &out), SEQC_OK);
    EXPECT_NE(dlist_pop_front(l, &out), SEQC_OK);
    EXPECT_TRUE(dlist_is_empty(l));
    EXPECT_EQ(dlist_front(l), nullptr);
    EXPECT_EQ(dlist_back(l), nullptr);
    growing_arena_destroy(a);
}

TEST(dlist, front_back_null_if_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_EQ(dlist_front(l), nullptr);
    EXPECT_EQ(dlist_back(l), nullptr);
    growing_arena_destroy(a);
}

TEST(dlist, single_element_front_equals_back)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    dlist_push_back(l, &v);
    EXPECT_EQ(*(int *)dlist_front(l), 42);
    EXPECT_EQ(*(int *)dlist_back(l), 42);
    growing_arena_destroy(a);
}

TEST(dlist, prev_links_are_correct)
{
    /* verify backward linkage by popping from the back repeatedly */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        dlist_push_back(l, &vals[i]);
    for (int i = 4; i >= 0; i--)
    {
        int out;
        EXPECT_EQ(dlist_pop_back(l, &out), SEQC_OK);
        EXPECT_EQ(out, vals[i]);
    }
    EXPECT_TRUE(dlist_is_empty(l));
    growing_arena_destroy(a);
}

TEST(dlist, free_does_not_crash)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int v = 1;
    dlist_push_back(l, &v);
    dlist_push_back(l, &v);
    dlist_free(l);
    EXPECT_TRUE(dlist_is_empty(l));
    growing_arena_destroy(a);
}

/* ---- dlist_clear ------------------------------------------------------- */

TEST(dlist, clear_empties_list)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        dlist_push_back(l, &i);
    dlist_clear(l);
    EXPECT_TRUE(dlist_is_empty(l));
    EXPECT_EQ(dlist_len(l), 0);
    growing_arena_destroy(a);
}

TEST(dlist, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &i);
    dlist_clear(l);
    int x = 99;
    dlist_push_back(l, &x);
    EXPECT_EQ(dlist_len(l), 1);
    EXPECT_EQ(*(int *)dlist_front(l), 99);
    growing_arena_destroy(a);
}

/* ---- OOM paths: an exhausted allocator must not crash ------------------ */

TEST(dlist, iter_oom_returns_empty)
{
    OomCtx ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    DList *l = dlist_create(sizeof(int), al);
    ASSERT_NE(l, nullptr);
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &i);
    ctx.remaining = 0; /* exhaust: the iterator's state alloc must fail */
    Iter it = dlist_iter(l);
    EXPECT_EQ(it.next, nullptr); /* empty iterator, not a NULL deref */
    iter_drop(&it);
    dlist_free(l);
}

TEST(dlist, iter_rev_oom_returns_empty)
{
    OomCtx ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    DList *l = dlist_create(sizeof(int), al);
    ASSERT_NE(l, nullptr);
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &i);
    ctx.remaining = 0;
    Iter it = dlist_iter_rev(l);
    EXPECT_EQ(it.next, nullptr);
    iter_drop(&it);
    dlist_free(l);
}
