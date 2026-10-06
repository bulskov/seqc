#include "ctt.h"
#include "oom_alloc.h"
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/list.h"
/* ---- tests ------------------------------------------------------------- */
TEST(list_is_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_TRUE(list_is_empty(l));
    ASSERT_EQ(0, list_len(l));
    growing_arena_destroy(a);
}
TEST(list_push_back_then_iter)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    ASSERT_EQ(3, list_len(l));
    iter_t it = list_iter(l);
    int got[3];
    size_t n = 0;
    while (it.next(&it, &got[n]))
        n++;
    iter_drop(&it);
    ASSERT_EQ(3, n);
    ASSERT_EQ(1, got[0]);
    ASSERT_EQ(2, got[1]);
    ASSERT_EQ(3, got[2]);
    growing_arena_destroy(a);
}
TEST(list_push_front_prepends)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int two = 2, one = 1;
    list_push_back(l, &two);
    list_push_front(l, &one);
    ASSERT_EQ(1, *(int *)list_front(l));
    ASSERT_EQ(2, *(int *)list_back(l));
    growing_arena_destroy(a);
}
TEST(list_pop_front_dequeues)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    int out;
    ASSERT_EQ(SEQC_OK, list_pop_front(l, &out));
    ASSERT_EQ(10, out);
    ASSERT_EQ(20, *(int *)list_front(l));
    ASSERT_EQ(2, list_len(l));
    growing_arena_destroy(a);
}
TEST(list_pop_front_empty_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int out;
    ASSERT_NE(SEQC_OK, list_pop_front(l, &out));
    growing_arena_destroy(a);
}
TEST(list_front_back_null_if_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    ASSERT_NULL(list_front(l));
    ASSERT_NULL(list_back(l));
    growing_arena_destroy(a);
}
TEST(list_single_element_front_equals_back)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    list_push_back(l, &v);
    ASSERT_EQ(42, *(int *)list_front(l));
    ASSERT_EQ(42, *(int *)list_back(l));
    growing_arena_destroy(a);
}
TEST(list_free_does_not_crash)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int v = 1;
    list_push_back(l, &v);
    list_push_back(l, &v);
    list_free(l);
    ASSERT_TRUE(list_is_empty(l));
    growing_arena_destroy(a);
}
/* ---- list_clear -------------------------------------------------------- */
TEST(list_clear_empties_list)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 4; i++)
        list_push_back(l, &i);
    list_clear(l);
    ASSERT_TRUE(list_is_empty(l));
    ASSERT_EQ(0, list_len(l));
    growing_arena_destroy(a);
}
TEST(list_clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        list_push_back(l, &i);
    list_clear(l);
    int x = 99;
    list_push_back(l, &x);
    ASSERT_EQ(1, list_len(l));
    ASSERT_EQ(99, *(int *)list_front(l));
    growing_arena_destroy(a);
}
/* ---- list_pop_back ----------------------------------------------------- */
TEST(list_pop_back_removes_last)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    int out;
    ASSERT_EQ(SEQC_OK, list_pop_back(l, &out));
    ASSERT_EQ(3, out);
    ASSERT_EQ(2, list_len(l));
    ASSERT_EQ(2, *(int *)list_back(l));
    growing_arena_destroy(a);
}
TEST(list_pop_back_single_element)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int v = 42;
    list_push_back(l, &v);
    int out;
    ASSERT_EQ(SEQC_OK, list_pop_back(l, &out));
    ASSERT_EQ(42, out);
    ASSERT_TRUE(list_is_empty(l));
    ASSERT_NULL(list_front(l));
    ASSERT_NULL(list_back(l));
    growing_arena_destroy(a);
}
TEST(list_pop_back_empty_returns_false)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 64);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int out;
    ASSERT_NE(SEQC_OK, list_pop_back(l, &out));
    growing_arena_destroy(a);
}
TEST(list_pop_back_null_out_allowed)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    list_t *l = list_create(sizeof(int), growing_arena_allocator(a));
    int v = 7;
    list_push_back(l, &v);
    ASSERT_EQ(SEQC_OK, list_pop_back(l, NULL));
    ASSERT_TRUE(list_is_empty(l));
    growing_arena_destroy(a);
}
/* ---- sys_allocator: exercises node-level free branches ----------------- */
TEST(list_sys_alloc_free_releases_nodes)
{
    allocator_t al = sys_allocator();
    list_t *l = list_create(sizeof(int), al);
    for (int i = 0; i < 4; i++)
        list_push_back(l, &i);
    /* list_free walks the list and frees each node (and the list handle
     * itself); releasing every node is verified by the leak sanitizer.
     * `l` is dangling after this call, so it must not be dereferenced. */
    list_free(l);
}
TEST(list_sys_alloc_pop_front_frees_node)
{
    allocator_t al = sys_allocator();
    list_t *l = list_create(sizeof(int), al);
    int v = 42;
    list_push_back(l, &v);
    int out;
    ASSERT_EQ(SEQC_OK, list_pop_front(l, &out));
    ASSERT_EQ(42, out);
    list_free(l);
}
TEST(list_sys_alloc_pop_back_frees_node)
{
    allocator_t al = sys_allocator();
    list_t *l = list_create(sizeof(int), al);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        list_push_back(l, &vals[i]);
    int out;
    ASSERT_EQ(SEQC_OK, list_pop_back(l, &out));
    ASSERT_EQ(3, out);
    ASSERT_EQ(2, list_len(l));
    list_free(l);
}
TEST(list_sys_alloc_clear_frees_all_nodes)
{
    allocator_t al = sys_allocator();
    list_t *l = list_create(sizeof(int), al);
    for (int i = 0; i < 5; i++)
        list_push_back(l, &i);
    list_clear(l);
    ASSERT_TRUE(list_is_empty(l));
    /* reuse after clear */
    int x = 99;
    list_push_back(l, &x);
    ASSERT_EQ(1, list_len(l));
    list_free(l);
}

/* ---- OOM paths: an exhausted allocator must not crash ------------------ */

TEST(list_iter_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    list_t *l = list_create(sizeof(int), al);
    ASSERT_NOT_NULL(l);
    for (int i = 0; i < 3; i++)
        list_push_back(l, &i);
    ctx.remaining = 0; /* exhaust: the iterator's state alloc must fail */
    iter_t it = list_iter(l);
    ASSERT_NULL(it.next); /* empty iterator, not a NULL deref */
    iter_drop(&it);
    list_free(l);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "list_test");
}
