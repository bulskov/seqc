#include <gtest/gtest.h>
#include "oom_alloc.h"
extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/stack.h"
}
/* ---- helpers ----------------------------------------------------------- */
static void push_ints(seqc_stack_t *s, const int *arr, size_t n)
{
    for (size_t i = 0; i < n; i++)
        stack_push(s, &arr[i]);
}
/* ---- tests ------------------------------------------------------------- */
TEST(stack, is_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_TRUE(stack_is_empty(s));
    EXPECT_EQ(stack_len(s), 0);
    growing_arena_destroy(a);
}
TEST(stack, push_pop_lifo_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    push_ints(s, vals, 3);
    EXPECT_EQ(stack_len(s), 3);
    int out;
    EXPECT_EQ(stack_pop(s, &out), SEQC_OK);
    EXPECT_EQ(out, 3);
    EXPECT_EQ(stack_pop(s, &out), SEQC_OK);
    EXPECT_EQ(out, 2);
    EXPECT_EQ(stack_pop(s, &out), SEQC_OK);
    EXPECT_EQ(out, 1);
    EXPECT_NE(stack_pop(s, &out), SEQC_OK);
    growing_arena_destroy(a);
}
TEST(stack, peek_does_not_consume)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    stack_push(s, &v);
    EXPECT_EQ(*(int *)stack_peek(s), 42);
    EXPECT_EQ(stack_len(s), 1); /* peek didn't pop */
    growing_arena_destroy(a);
}
TEST(stack, peek_empty_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    EXPECT_EQ(stack_peek(s), nullptr);
    growing_arena_destroy(a);
}
TEST(stack, pop_empty_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int out;
    EXPECT_NE(stack_pop(s, &out), SEQC_OK);
    growing_arena_destroy(a);
}
TEST(stack, iter_bottom_to_top)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    push_ints(s, vals, 3);
    /* iter goes bottom→top: 10, 20, 30 */
    iter_t it = stack_iter(s);
    int got[3];
    size_t i = 0;
    while (it.next(&it, &got[i]))
        i++;
    iter_drop(&it);
    EXPECT_EQ(i, 3);
    EXPECT_EQ(got[0], 10);
    EXPECT_EQ(got[1], 20);
    EXPECT_EQ(got[2], 30);
    growing_arena_destroy(a);
}
TEST(stack, pop_null_out_ok)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int v = 7;
    stack_push(s, &v);
    EXPECT_EQ(stack_pop(s, NULL), SEQC_OK); /* just discard */
    EXPECT_TRUE(stack_is_empty(s));
    growing_arena_destroy(a);
}
/* ---- stack_clear ------------------------------------------------------- */
TEST(stack, clear_empties_stack)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        stack_push(s, &i);
    stack_clear(s);
    EXPECT_TRUE(stack_is_empty(s));
    EXPECT_EQ(stack_len(s), 0);
    growing_arena_destroy(a);
}
TEST(stack, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        stack_push(s, &i);
    stack_clear(s);
    int x = 99;
    stack_push(s, &x);
    int out;
    EXPECT_EQ(stack_pop(s, &out), SEQC_OK);
    EXPECT_EQ(out, 99);
    growing_arena_destroy(a);
}
/* ---- stack_iter_rev ---------------------------------------------------- */
TEST(stack, iter_rev_top_to_bottom)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    push_ints(s, vals, 3);
    /* iter_rev goes top→bottom: 30, 20, 10 */
    iter_t it = stack_iter_rev(s);
    int got[3];
    size_t i = 0;
    while (it.next(&it, &got[i]))
        i++;
    iter_drop(&it);
    EXPECT_EQ(i, 3);
    EXPECT_EQ(got[0], 30);
    EXPECT_EQ(got[1], 20);
    EXPECT_EQ(got[2], 10);
    growing_arena_destroy(a);
}
TEST(stack, iter_rev_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    iter_t it = stack_iter_rev(s);
    int v;
    EXPECT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises stack_free ------------------------------- */
TEST(stack, sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    seqc_stack_t *s = stack_create(sizeof(int), al);
    for (int i = 0; i < 4; i++)
        stack_push(s, &i);
    EXPECT_EQ(stack_len(s), 4);
    stack_free(s);
    /* stack_free releases all memory — verified by sys_allocator not leaking */
}
