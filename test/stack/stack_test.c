#include <criterion/criterion.h>
#include "oom_alloc.h"
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/stack.h"
/* ---- helpers ----------------------------------------------------------- */
static void push_ints(Stack *s, const int *arr, size_t n)
{
    for (size_t i = 0; i < n; i++)
        stack_push(s, &arr[i]);
}
/* ---- tests ------------------------------------------------------------- */
Test(stack, is_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    cr_assert(stack_is_empty(s));
    cr_assert_eq(stack_len(s), 0);
    growing_arena_destroy(a);
}
Test(stack, push_pop_lifo_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    push_ints(s, vals, 3);
    cr_assert_eq(stack_len(s), 3);
    int out;
    cr_assert_eq(stack_pop(s, &out), SEQC_OK);
    cr_assert_eq(out, 3);
    cr_assert_eq(stack_pop(s, &out), SEQC_OK);
    cr_assert_eq(out, 2);
    cr_assert_eq(stack_pop(s, &out), SEQC_OK);
    cr_assert_eq(out, 1);
    cr_assert_neq(stack_pop(s, &out), SEQC_OK);
    growing_arena_destroy(a);
}
Test(stack, peek_does_not_consume)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    stack_push(s, &v);
    cr_assert_eq(*(int *)stack_peek(s), 42);
    cr_assert_eq(stack_len(s), 1); /* peek didn't pop */
    growing_arena_destroy(a);
}
Test(stack, peek_empty_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    cr_assert_null(stack_peek(s));
    growing_arena_destroy(a);
}
Test(stack, pop_empty_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int out;
    cr_assert_neq(stack_pop(s, &out), SEQC_OK);
    growing_arena_destroy(a);
}
Test(stack, iter_bottom_to_top)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    push_ints(s, vals, 3);
    /* iter goes bottom→top: 10, 20, 30 */
    Iter it = stack_iter(s);
    int got[3];
    size_t i = 0;
    while (it.next(&it, &got[i]))
        i++;
    iter_drop(&it);
    cr_assert_eq(i, 3);
    cr_assert_eq(got[0], 10);
    cr_assert_eq(got[1], 20);
    cr_assert_eq(got[2], 30);
    growing_arena_destroy(a);
}
Test(stack, pop_null_out_ok)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int v = 7;
    stack_push(s, &v);
    cr_assert_eq(stack_pop(s, NULL), SEQC_OK); /* just discard */
    cr_assert(stack_is_empty(s));
    growing_arena_destroy(a);
}
/* ---- stack_clear ------------------------------------------------------- */
Test(stack, clear_empties_stack)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        stack_push(s, &i);
    stack_clear(s);
    cr_assert(stack_is_empty(s));
    cr_assert_eq(stack_len(s), 0);
    growing_arena_destroy(a);
}
Test(stack, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        stack_push(s, &i);
    stack_clear(s);
    int x = 99;
    stack_push(s, &x);
    int out;
    cr_assert_eq(stack_pop(s, &out), SEQC_OK);
    cr_assert_eq(out, 99);
    growing_arena_destroy(a);
}
/* ---- stack_iter_rev ---------------------------------------------------- */
Test(stack, iter_rev_top_to_bottom)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    push_ints(s, vals, 3);
    /* iter_rev goes top→bottom: 30, 20, 10 */
    Iter it = stack_iter_rev(s);
    int got[3];
    size_t i = 0;
    while (it.next(&it, &got[i]))
        i++;
    iter_drop(&it);
    cr_assert_eq(i, 3);
    cr_assert_eq(got[0], 30);
    cr_assert_eq(got[1], 20);
    cr_assert_eq(got[2], 10);
    growing_arena_destroy(a);
}
Test(stack, iter_rev_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Stack *s = stack_create(sizeof(int), growing_arena_allocator(a));
    Iter it = stack_iter_rev(s);
    int v;
    cr_assert(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises stack_free ------------------------------- */
Test(stack, sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    Stack *s = stack_create(sizeof(int), al);
    for (int i = 0; i < 4; i++)
        stack_push(s, &i);
    cr_assert_eq(stack_len(s), 4);
    stack_free(s);
    /* stack_free releases all memory — verified by sys_allocator not leaking */
}
