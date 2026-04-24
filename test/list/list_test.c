#include <criterion/criterion.h>

#include "seqc/arena.h"
#include "seqc/list.h"

/* ---- tests ------------------------------------------------------------- */

Test(list, is_empty_on_create)
{
    Arena *a = arena_create(256);
    List *l = list_create(sizeof(int), arena_allocator(a));
    cr_assert(list_is_empty(l));
    cr_assert_eq(list_len(l), 0);
    arena_free(a);
}

Test(list, push_back_then_iter)
{
    Arena *a = arena_create(512);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    cr_assert_eq(list_len(l), 3);
    Iter it = list_iter(l);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 3);
    cr_assert_eq(got[0], 1);
    cr_assert_eq(got[1], 2);
    cr_assert_eq(got[2], 3);
    arena_free(a);
}

Test(list, push_front_prepends)
{
    Arena *a = arena_create(512);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int two = 2, one = 1;
    list_push_back(l, &two);
    list_push_front(l, &one);
    cr_assert_eq(*(int *)list_front(l), 1);
    cr_assert_eq(*(int *)list_back(l), 2);
    arena_free(a);
}

Test(list, pop_front_dequeues)
{
    Arena *a = arena_create(512);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    int out;
    cr_assert_eq(list_pop_front(l, &out), SEQC_OK);
    cr_assert_eq(out, 10);
    cr_assert_eq(*(int *)list_front(l), 20);
    cr_assert_eq(list_len(l), 2);
    arena_free(a);
}

Test(list, pop_front_empty_returns_0)
{
    Arena *a = arena_create(64);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int out;
    cr_assert_neq(list_pop_front(l, &out), SEQC_OK);
    arena_free(a);
}

Test(list, front_back_null_if_empty)
{
    Arena *a = arena_create(64);
    List *l = list_create(sizeof(int), arena_allocator(a));
    cr_assert_null(list_front(l));
    cr_assert_null(list_back(l));
    arena_free(a);
}

Test(list, single_element_front_equals_back)
{
    Arena *a = arena_create(256);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int v = 42;
    list_push_back(l, &v);
    cr_assert_eq(*(int *)list_front(l), 42);
    cr_assert_eq(*(int *)list_back(l), 42);
    arena_free(a);
}

Test(list, free_does_not_crash)
{
    Arena *a = arena_create(512);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int v = 1;
    list_push_back(l, &v);
    list_push_back(l, &v);
    list_free(l);
    cr_assert(list_is_empty(l));
    arena_free(a);
}

/* ---- list_clear -------------------------------------------------------- */

Test(list, clear_empties_list)
{
    Arena *a = arena_create(256);
    List *l = list_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 4; i++)
        list_push_back(l, &i);
    list_clear(l);
    cr_assert(list_is_empty(l));
    cr_assert_eq(list_len(l), 0);
    arena_free(a);
}

Test(list, clear_allows_reuse)
{
    Arena *a = arena_create(512);
    List *l = list_create(sizeof(int), arena_allocator(a));
    for (int i = 0; i < 3; i++)
        list_push_back(l, &i);
    list_clear(l);
    int x = 99;
    list_push_back(l, &x);
    cr_assert_eq(list_len(l), 1);
    cr_assert_eq(*(int *)list_front(l), 99);
    arena_free(a);
}

/* ---- list_pop_back ----------------------------------------------------- */

Test(list, pop_back_removes_last)
{
    Arena *a = arena_create(512);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    int out;
    cr_assert_eq(list_pop_back(l, &out), SEQC_OK);
    cr_assert_eq(out, 3);
    cr_assert_eq(list_len(l), 2);
    cr_assert_eq(*(int *)list_back(l), 2);
    arena_free(a);
}

Test(list, pop_back_single_element)
{
    Arena *a = arena_create(256);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int v = 42;
    list_push_back(l, &v);
    int out;
    cr_assert_eq(list_pop_back(l, &out), SEQC_OK);
    cr_assert_eq(out, 42);
    cr_assert(list_is_empty(l));
    cr_assert_null(list_front(l));
    cr_assert_null(list_back(l));
    arena_free(a);
}

Test(list, pop_back_empty_returns_false)
{
    Arena *a = arena_create(64);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int out;
    cr_assert_neq(list_pop_back(l, &out), SEQC_OK);
    arena_free(a);
}

Test(list, pop_back_null_out_allowed)
{
    Arena *a = arena_create(256);
    List *l = list_create(sizeof(int), arena_allocator(a));
    int v = 7;
    list_push_back(l, &v);
    cr_assert_eq(list_pop_back(l, NULL), SEQC_OK);
    cr_assert(list_is_empty(l));
    arena_free(a);
}

/* ---- sys_allocator: exercises node-level free branches ----------------- */

Test(list, sys_alloc_free_releases_nodes)
{
    Allocator al = sys_allocator();
    List *l = list_create(sizeof(int), al);
    for (int i = 0; i < 4; i++)
        list_push_back(l, &i);
    /* list_free walks the list and frees each node */
    list_free(l);
    cr_assert(list_is_empty(l));
}

Test(list, sys_alloc_pop_front_frees_node)
{
    Allocator al = sys_allocator();
    List *l = list_create(sizeof(int), al);
    int v = 42;
    list_push_back(l, &v);
    int out;
    cr_assert_eq(list_pop_front(l, &out), SEQC_OK);
    cr_assert_eq(out, 42);
    list_free(l);
}

Test(list, sys_alloc_pop_back_frees_node)
{
    Allocator al = sys_allocator();
    List *l = list_create(sizeof(int), al);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    int out;
    cr_assert_eq(list_pop_back(l, &out), SEQC_OK);
    cr_assert_eq(out, 3);
    cr_assert_eq(list_len(l), 2);
    list_free(l);
}

Test(list, sys_alloc_clear_frees_all_nodes)
{
    Allocator al = sys_allocator();
    List *l = list_create(sizeof(int), al);
    for (int i = 0; i < 5; i++)
        list_push_back(l, &i);
    list_clear(l);
    cr_assert(list_is_empty(l));
    /* reuse after clear */
    int x = 99;
    list_push_back(l, &x);
    cr_assert_eq(list_len(l), 1);
    list_free(l);
}
