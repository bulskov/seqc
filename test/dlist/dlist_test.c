#include <criterion/criterion.h>

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/dlist.h"

/* ---- tests ------------------------------------------------------------- */

Test(dlist, is_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    cr_assert(dlist_is_empty(l));
    cr_assert_eq(dlist_len(l), 0);
    growing_arena_destroy(a);
}

Test(dlist, push_back_iter_forward)
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
    cr_assert_eq(n, 3);
    cr_assert_eq(got[0], 1);
    cr_assert_eq(got[1], 2);
    cr_assert_eq(got[2], 3);
    growing_arena_destroy(a);
}

Test(dlist, iter_rev)
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
    cr_assert_eq(n, 3);
    cr_assert_eq(got[0], 3);
    cr_assert_eq(got[1], 2);
    cr_assert_eq(got[2], 1);
    growing_arena_destroy(a);
}

Test(dlist, push_front_prepends)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int two = 2, one = 1;
    dlist_push_back(l, &two);
    dlist_push_front(l, &one);
    cr_assert_eq(*(int *)dlist_front(l), 1);
    cr_assert_eq(*(int *)dlist_back(l), 2);
    growing_arena_destroy(a);
}

Test(dlist, pop_front_removes_head)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &vals[i]);
    int out;
    cr_assert_eq(dlist_pop_front(l, &out), SEQC_OK);
    cr_assert_eq(out, 10);
    cr_assert_eq(*(int *)dlist_front(l), 20);
    cr_assert_eq(dlist_len(l), 2);
    growing_arena_destroy(a);
}

Test(dlist, pop_back_removes_tail)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &vals[i]);
    int out;
    cr_assert_eq(dlist_pop_back(l, &out), SEQC_OK);
    cr_assert_eq(out, 30);
    cr_assert_eq(*(int *)dlist_back(l), 20);
    cr_assert_eq(dlist_len(l), 2);
    growing_arena_destroy(a);
}

Test(dlist, pop_until_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int v = 1;
    dlist_push_back(l, &v);
    int out;
    cr_assert_eq(dlist_pop_front(l, &out), SEQC_OK);
    cr_assert_neq(dlist_pop_front(l, &out), SEQC_OK);
    cr_assert(dlist_is_empty(l));
    cr_assert_null(dlist_front(l));
    cr_assert_null(dlist_back(l));
    growing_arena_destroy(a);
}

Test(dlist, front_back_null_if_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    cr_assert_null(dlist_front(l));
    cr_assert_null(dlist_back(l));
    growing_arena_destroy(a);
}

Test(dlist, single_element_front_equals_back)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    dlist_push_back(l, &v);
    cr_assert_eq(*(int *)dlist_front(l), 42);
    cr_assert_eq(*(int *)dlist_back(l), 42);
    growing_arena_destroy(a);
}

Test(dlist, prev_links_are_correct)
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
        cr_assert_eq(dlist_pop_back(l, &out), SEQC_OK);
        cr_assert_eq(out, vals[i]);
    }
    cr_assert(dlist_is_empty(l));
    growing_arena_destroy(a);
}

Test(dlist, free_does_not_crash)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    int v = 1;
    dlist_push_back(l, &v);
    dlist_push_back(l, &v);
    dlist_free(l);
    cr_assert(dlist_is_empty(l));
    growing_arena_destroy(a);
}

/* ---- dlist_clear ------------------------------------------------------- */

Test(dlist, clear_empties_list)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        dlist_push_back(l, &i);
    dlist_clear(l);
    cr_assert(dlist_is_empty(l));
    cr_assert_eq(dlist_len(l), 0);
    growing_arena_destroy(a);
}

Test(dlist, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    DList *l = dlist_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        dlist_push_back(l, &i);
    dlist_clear(l);
    int x = 99;
    dlist_push_back(l, &x);
    cr_assert_eq(dlist_len(l), 1);
    cr_assert_eq(*(int *)dlist_front(l), 99);
    growing_arena_destroy(a);
}
