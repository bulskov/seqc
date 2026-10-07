#include "ctt.h"
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "oom_alloc.h"
#include "seqc/stack.h"
/* ---- helpers ----------------------------------------------------------- */
static void push_ints(seqc_stack_t *s, const int *arr, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        stack_push(s, &arr[i]);
    }
}
/* ---- tests ------------------------------------------------------------- */
TEST(stack_is_empty_on_create)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_TRUE(stack_is_empty(s));
    ASSERT_EQ(0, stack_len(s));
    growing_arena_destroy(a);
}
TEST(stack_push_pop_lifo_order)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    push_ints(s, vals, 3);
    ASSERT_EQ(3, stack_len(s));
    int out;
    ASSERT_EQ(SEQC_OK, stack_pop(s, &out));
    ASSERT_EQ(3, out);
    ASSERT_EQ(SEQC_OK, stack_pop(s, &out));
    ASSERT_EQ(2, out);
    ASSERT_EQ(SEQC_OK, stack_pop(s, &out));
    ASSERT_EQ(1, out);
    ASSERT_NE(SEQC_OK, stack_pop(s, &out));
    growing_arena_destroy(a);
}
TEST(stack_peek_does_not_consume)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    stack_push(s, &v);
    ASSERT_EQ(42, *(int *)stack_peek(s));
    ASSERT_EQ(1, stack_len(s)); /* peek didn't pop */
    growing_arena_destroy(a);
}
TEST(stack_peek_empty_returns_null)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_NULL(stack_peek(s));
    growing_arena_destroy(a);
}
TEST(stack_pop_empty_returns_0)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 64);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int out;
    ASSERT_NE(SEQC_OK, stack_pop(s, &out));
    growing_arena_destroy(a);
}
TEST(stack_iter_bottom_to_top)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    push_ints(s, vals, 3);
    /* iter goes bottom→top: 10, 20, 30 */
    iter_t it = stack_iter(s);
    int got[3];
    size_t i = 0;
    while (it.next(&it, &got[i]))
    {
        i++;
    }
    iter_drop(&it);
    ASSERT_EQ(3, i);
    ASSERT_EQ(10, got[0]);
    ASSERT_EQ(20, got[1]);
    ASSERT_EQ(30, got[2]);
    growing_arena_destroy(a);
}
TEST(stack_pop_null_out_ok)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int v = 7;
    stack_push(s, &v);
    ASSERT_EQ(SEQC_OK, stack_pop(s, NULL)); /* just discard */
    ASSERT_TRUE(stack_is_empty(s));
    growing_arena_destroy(a);
}
/* ---- stack_clear ------------------------------------------------------- */
TEST(stack_clear_empties_stack)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
    {
        stack_push(s, &i);
    }
    stack_clear(s);
    ASSERT_TRUE(stack_is_empty(s));
    ASSERT_EQ(0, stack_len(s));
    growing_arena_destroy(a);
}
TEST(stack_clear_allows_reuse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        stack_push(s, &i);
    }
    stack_clear(s);
    int x = 99;
    stack_push(s, &x);
    int out;
    ASSERT_EQ(SEQC_OK, stack_pop(s, &out));
    ASSERT_EQ(99, out);
    growing_arena_destroy(a);
}
/* ---- stack_iter_rev ---------------------------------------------------- */
TEST(stack_iter_rev_top_to_bottom)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    push_ints(s, vals, 3);
    /* iter_rev goes top→bottom: 30, 20, 10 */
    iter_t it = stack_iter_rev(s);
    int got[3];
    size_t i = 0;
    while (it.next(&it, &got[i]))
    {
        i++;
    }
    iter_drop(&it);
    ASSERT_EQ(3, i);
    ASSERT_EQ(30, got[0]);
    ASSERT_EQ(20, got[1]);
    ASSERT_EQ(10, got[2]);
    growing_arena_destroy(a);
}
TEST(stack_iter_rev_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    seqc_stack_t *s = stack_create(sizeof(int), growing_arena_allocator(a));
    iter_t it = stack_iter_rev(s);
    int v;
    ASSERT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises stack_free ------------------------------- */
TEST(stack_sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    seqc_stack_t *s = stack_create(sizeof(int), al);
    for (int i = 0; i < 4; i++)
    {
        stack_push(s, &i);
    }
    ASSERT_EQ(4, stack_len(s));
    stack_free(s);
    /* stack_free releases all memory — verified by sys_allocator not leaking */
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "stack_test");
}
