#include <gtest/gtest.h>
#include "oom_alloc.h"
extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/list.h"
}
/* ---- tests ------------------------------------------------------------- */
TEST(list, is_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_TRUE(list_is_empty(l));
    EXPECT_EQ(list_len(l), 0);
    growing_arena_destroy(a);
}
TEST(list, push_back_then_iter)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    EXPECT_EQ(list_len(l), 3);
    Iter it = list_iter(l);
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
TEST(list, push_front_prepends)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int two = 2, one = 1;
    list_push_back(l, &two);
    list_push_front(l, &one);
    EXPECT_EQ(*(int *)list_front(l), 1);
    EXPECT_EQ(*(int *)list_back(l), 2);
    growing_arena_destroy(a);
}
TEST(list, pop_front_dequeues)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    int out;
    EXPECT_EQ(list_pop_front(l, &out), SEQC_OK);
    EXPECT_EQ(out, 10);
    EXPECT_EQ(*(int *)list_front(l), 20);
    EXPECT_EQ(list_len(l), 2);
    growing_arena_destroy(a);
}
TEST(list, pop_front_empty_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int out;
    EXPECT_NE(list_pop_front(l, &out), SEQC_OK);
    growing_arena_destroy(a);
}
TEST(list, front_back_null_if_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_EQ(list_front(l), nullptr);
    EXPECT_EQ(list_back(l), nullptr);
    growing_arena_destroy(a);
}
TEST(list, single_element_front_equals_back)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    list_push_back(l, &v);
    EXPECT_EQ(*(int *)list_front(l), 42);
    EXPECT_EQ(*(int *)list_back(l), 42);
    growing_arena_destroy(a);
}
TEST(list, free_does_not_crash)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int v = 1;
    list_push_back(l, &v);
    list_push_back(l, &v);
    list_free(l);
    EXPECT_TRUE(list_is_empty(l));
    growing_arena_destroy(a);
}
/* ---- list_clear -------------------------------------------------------- */
TEST(list, clear_empties_list)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        list_push_back(l, &i);
    list_clear(l);
    EXPECT_TRUE(list_is_empty(l));
    EXPECT_EQ(list_len(l), 0);
    growing_arena_destroy(a);
}
TEST(list, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        list_push_back(l, &i);
    list_clear(l);
    int x = 99;
    list_push_back(l, &x);
    EXPECT_EQ(list_len(l), 1);
    EXPECT_EQ(*(int *)list_front(l), 99);
    growing_arena_destroy(a);
}
/* ---- list_pop_back ----------------------------------------------------- */
TEST(list, pop_back_removes_last)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    int out;
    EXPECT_EQ(list_pop_back(l, &out), SEQC_OK);
    EXPECT_EQ(out, 3);
    EXPECT_EQ(list_len(l), 2);
    EXPECT_EQ(*(int *)list_back(l), 2);
    growing_arena_destroy(a);
}
TEST(list, pop_back_single_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    list_push_back(l, &v);
    int out;
    EXPECT_EQ(list_pop_back(l, &out), SEQC_OK);
    EXPECT_EQ(out, 42);
    EXPECT_TRUE(list_is_empty(l));
    EXPECT_EQ(list_front(l), nullptr);
    EXPECT_EQ(list_back(l), nullptr);
    growing_arena_destroy(a);
}
TEST(list, pop_back_empty_returns_false)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int out;
    EXPECT_NE(list_pop_back(l, &out), SEQC_OK);
    growing_arena_destroy(a);
}
TEST(list, pop_back_null_out_allowed)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    List *l = list_create(sizeof(int), growing_arena_allocator(a));
    int v = 7;
    list_push_back(l, &v);
    EXPECT_EQ(list_pop_back(l, NULL), SEQC_OK);
    EXPECT_TRUE(list_is_empty(l));
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises node-level free branches ----------------- */
TEST(list, sys_alloc_free_releases_nodes)
{
    allocator_t al = sys_allocator();
    List *l = list_create(sizeof(int), al);
    for (int i = 0; i < 4; i++)
        list_push_back(l, &i);
    /* list_free walks the list and frees each node (and the List handle
     * itself); releasing every node is verified by the leak sanitizer.
     * `l` is dangling after this call, so it must not be dereferenced. */
    list_free(l);
}
TEST(list, sys_alloc_pop_front_frees_node)
{
    allocator_t al = sys_allocator();
    List *l = list_create(sizeof(int), al);
    int v = 42;
    list_push_back(l, &v);
    int out;
    EXPECT_EQ(list_pop_front(l, &out), SEQC_OK);
    EXPECT_EQ(out, 42);
    list_free(l);
}
TEST(list, sys_alloc_pop_back_frees_node)
{
    allocator_t al = sys_allocator();
    List *l = list_create(sizeof(int), al);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    int out;
    EXPECT_EQ(list_pop_back(l, &out), SEQC_OK);
    EXPECT_EQ(out, 3);
    EXPECT_EQ(list_len(l), 2);
    list_free(l);
}
TEST(list, sys_alloc_clear_frees_all_nodes)
{
    allocator_t al = sys_allocator();
    List *l = list_create(sizeof(int), al);
    for (int i = 0; i < 5; i++)
        list_push_back(l, &i);
    list_clear(l);
    EXPECT_TRUE(list_is_empty(l));
    /* reuse after clear */
    int x = 99;
    list_push_back(l, &x);
    EXPECT_EQ(list_len(l), 1);
    list_free(l);
}

/* ---- OOM paths: an exhausted allocator must not crash ------------------ */

TEST(list, iter_oom_returns_empty)
{
    OomCtx ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    List *l = list_create(sizeof(int), al);
    ASSERT_NE(l, nullptr);
    for (int i = 0; i < 3; i++)
        list_push_back(l, &i);
    ctx.remaining = 0; /* exhaust: the iterator's state alloc must fail */
    Iter it = list_iter(l);
    EXPECT_EQ(it.next, nullptr); /* empty iterator, not a NULL deref */
    iter_drop(&it);
    list_free(l);
}
