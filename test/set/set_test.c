#include <gtest/gtest.h>

extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/set.h"
}
#include "../oom_alloc.h"

/* ---- hash/eq for int keys ---------------------------------------------- */

static size_t int_hash(const void *key, size_t key_size)
{
    (void)key_size;
    /* Knuth multiplicative hash */
    return (size_t)(*(const unsigned int *)key) * 2654435761u;
}

static bool int_eq(const void *a, const void *b, size_t key_size)
{
    (void)key_size;
    return *(const int *)a == *(const int *)b;
}

/* ---- tests ------------------------------------------------------------- */

TEST(set, is_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    EXPECT_EQ(set_len(s), 0);
    growing_arena_destroy(a);
}

TEST(set, add_contains)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int v1 = 1, v2 = 2, v3 = 42;
    EXPECT_EQ(set_add(s, &v1), SEQC_OK);
    EXPECT_EQ(set_add(s, &v2), SEQC_OK);
    EXPECT_EQ(set_add(s, &v3), SEQC_OK);
    EXPECT_TRUE(set_contains(s, &v1));
    EXPECT_TRUE(set_contains(s, &v2));
    EXPECT_TRUE(set_contains(s, &v3));
    EXPECT_EQ(set_len(s), 3);
    growing_arena_destroy(a);
}

TEST(set, add_duplicate_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int v = 7;
    EXPECT_EQ(set_add(s, &v), SEQC_OK);
    EXPECT_NE(set_add(s, &v), SEQC_OK); /* already present */
    EXPECT_EQ(set_len(s), 1);
    growing_arena_destroy(a);
}

TEST(set, remove_existing)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int v = 5;
    set_add(s, &v);
    EXPECT_EQ(set_remove(s, &v), SEQC_OK);
    EXPECT_FALSE(set_contains(s, &v));
    EXPECT_EQ(set_len(s), 0);
    growing_arena_destroy(a);
}

TEST(set, remove_nonexistent_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int v = 99;
    EXPECT_NE(set_remove(s, &v), SEQC_OK);
    growing_arena_destroy(a);
}

TEST(set, does_not_contain_absent_key)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int present = 1, absent = 2;
    set_add(s, &present);
    EXPECT_FALSE(set_contains(s, &absent));
    growing_arena_destroy(a);
}

TEST(set, iter_yields_all_elements)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int vals[] = {10, 20, 30, 40};
    for (int i = 0; i < 4; i++)
        set_add(s, &vals[i]);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    size_t count = iter_count(set_iter(s));
    EXPECT_EQ(count, 4);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(set, grow_beyond_initial_cap)
{
    /* Add 20 elements to force a resize (load factor 0.75 of initial cap 16) */
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 20; i++)
        set_add(s, &i);
    EXPECT_EQ(set_len(s), 20);
    for (int i = 0; i < 20; i++)
        EXPECT_TRUE(set_contains(s, &i));
    growing_arena_destroy(a);
}

/* ---- set_clear --------------------------------------------------------- */

TEST(set, clear_empties_set)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        set_add(s, &i);
    set_clear(s);
    EXPECT_EQ(set_len(s), 0);
    for (int i = 0; i < 5; i++)
        EXPECT_TRUE(!set_contains(s, &i));
    growing_arena_destroy(a);
}

TEST(set, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
        set_add(s, &i);
    set_clear(s);
    int x = 42;
    EXPECT_EQ(set_add(s, &x), SEQC_OK);
    EXPECT_EQ(set_len(s), 1);
    EXPECT_TRUE(set_contains(s, &x));
    growing_arena_destroy(a);
}

/* ---- set_iter_rev ------------------------------------------------------- */

TEST(set, iter_rev_yields_all_elements)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
        set_add(s, &i);
    Iter it = set_iter_rev(s);
    size_t n = 0;
    int v;
    while (it.next(&it, &v))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 5);
    growing_arena_destroy(a);
}

TEST(set, iter_rev_empty_set)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Iter it = set_iter_rev(s);
    int v;
    EXPECT_TRUE(!it.next(&it, &v));
    iter_drop(&it);
    growing_arena_destroy(a);
}

/* ---- collision / probe-chain tests ------------------------------------- */

/* Forces every element to the same home slot. */
static size_t always_zero_set_hash(const void *key, size_t key_size)
{
    (void)key;
    (void)key_size;
    return 0;
}

/*
 * Maps keys 1,4 → slot 0, key 2 → slot 1, key 3 → slot 2.
 * Inserting in order 1,2,3,4 causes Robin Hood displacement when key 4
 * (home=0) reaches slot 1 where key 2 sits with PSL=1 < key4's PSL=2.
 */
static size_t robin_hood_set_hash(const void *key, size_t key_size)
{
    (void)key_size;
    switch (*(const int *)key)
    {
    case 1:
        return 0;
    case 2:
        return 1;
    case 3:
        return 2;
    case 4:
        return 0;
    default:
        return (size_t)(unsigned)(*(const int *)key);
    }
}

/*
 * All keys share home slot 0.
 * Exercises the probe loop in set_insert_raw and set_contains.
 */
TEST(set, collision_probe_insert_and_contains)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(
        sizeof(int), always_zero_set_hash, int_eq, growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
        EXPECT_EQ(set_add(s, &i), SEQC_OK);
    EXPECT_EQ(set_len(s), 4);
    for (int i = 1; i <= 4; i++)
        EXPECT_TRUE(set_contains(s, &i));
    growing_arena_destroy(a);
}

/*
 * Robin Hood displacement: key 4 (home=0) probes past key 1, then steals
 * slot 1 from key 2 (PSL=1 < incoming PSL=2), cascading key 2 and key 3
 * rightward.  All four elements must remain members after the cascade.
 */
TEST(set, collision_robin_hood_displacement)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(
        sizeof(int), robin_hood_set_hash, int_eq, growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
        EXPECT_EQ(set_add(s, &i), SEQC_OK);
    EXPECT_EQ(set_len(s), 4);
    for (int i = 1; i <= 4; i++)
        EXPECT_TRUE(set_contains(s, &i));
    growing_arena_destroy(a);
}

/*
 * Remove an element that is NOT at its home slot (must probe to find it),
 * then exercise the backward-shift loop (nb->psl > 1) to compact the chain.
 * The remaining elements must still be findable.
 */
TEST(set, collision_remove_probe_and_backward_shift)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(
        sizeof(int), always_zero_set_hash, int_eq, growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
        set_add(s, &i);
    /* elem 3 sits at slot 2 (home=0): remove requires probing slots 0,1 first,
     * then backward-shifts elem 4 into the vacated slot. */
    int k = 3;
    EXPECT_EQ(set_remove(s, &k), SEQC_OK);
    EXPECT_TRUE(!set_contains(s, &k));
    for (int i = 1; i <= 4; i++)
    {
        if (i == 3)
            continue;
        EXPECT_TRUE(set_contains(s, &i));
    }
    EXPECT_EQ(set_len(s), 3);
    growing_arena_destroy(a);
}

/*
 * set_remove on a non-empty set where the key is absent; exercises the
 * b->psl == 0 early-exit path (distinct from the s->len == 0 guard).
 */
TEST(set, remove_absent_hits_empty_slot)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(
        sizeof(int), always_zero_set_hash, int_eq, growing_arena_allocator(a));
    int present = 1;
    set_add(s, &present);
    /* key 99 also hashes to slot 0 but is not in the set; after probing past
     * present we hit an empty slot and must return false. */
    int absent = 99;
    EXPECT_NE(set_remove(s, &absent), SEQC_OK);
    EXPECT_EQ(set_len(s), 1);
    growing_arena_destroy(a);
}

/* ---- sys_allocator: exercises all allocator.free branches -------------- */

TEST(set, sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    Set *s = set_create(sizeof(int), int_hash, int_eq, al);
    for (int i = 0; i < 5; i++)
        set_add(s, &i);
    EXPECT_EQ(set_len(s), 5);
    set_free(s);
    /* memory released — verified by sys_allocator not leaking */
}

TEST(set, sys_alloc_clear_frees_keys)
{
    allocator_t al = sys_allocator();
    Set *s = set_create(sizeof(int), int_hash, int_eq, al);
    for (int i = 0; i < 4; i++)
        set_add(s, &i);
    set_clear(s);
    EXPECT_EQ(set_len(s), 0);
    /* set is still usable after clear */
    int x = 42;
    EXPECT_EQ(set_add(s, &x), SEQC_OK);
    EXPECT_TRUE(set_contains(s, &x));
    set_free(s);
}

TEST(set, sys_alloc_remove_frees_key)
{
    allocator_t al = sys_allocator();
    Set *s = set_create(sizeof(int), int_hash, int_eq, al);
    int v = 7;
    set_add(s, &v);
    EXPECT_EQ(set_remove(s, &v), SEQC_OK);
    EXPECT_TRUE(!set_contains(s, &v));
    set_free(s);
}

TEST(set, is_healthy_normal_load)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 50; i++)
        set_add(s, &i);
    EXPECT_TRUE(set_is_healthy(s));
    growing_arena_destroy(a);
}

TEST(set, audit_normal_load)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 50; i++)
        set_add(s, &i);
    SetStats st = set_audit(s);
    EXPECT_EQ(st.len, 50);
    EXPECT_TRUE(st.cap >= 50);
    EXPECT_TRUE(st.load_factor > 0.0 && st.load_factor <= 1.0);
    EXPECT_TRUE(st.max_psl >= 1);
    EXPECT_TRUE(st.mean_psl >= 1.0);
    EXPECT_TRUE(st.is_healthy);
    growing_arena_destroy(a);
}

/* ---- set algebra ------------------------------------------------------- */

TEST(set, union_disjoint)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++) set_add(s1, &i);
    for (int i = 5; i < 10; i++) set_add(s2, &i);
    EXPECT_EQ(set_union(dst, s1, s2), SEQC_OK);
    EXPECT_EQ(set_len(dst), 10);
    for (int i = 0; i < 10; i++)
        EXPECT_TRUE(set_contains(dst, &i));
    growing_arena_destroy(a);
}

TEST(set, union_overlapping)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    /* s1 = {1,2,3}, s2 = {2,3,4} => union = {1,2,3,4} */
    int v[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) set_add(s1, &v[i]);
    int v2[] = {2, 3, 4};
    for (int i = 0; i < 3; i++) set_add(s2, &v2[i]);
    EXPECT_EQ(set_union(dst, s1, s2), SEQC_OK);
    EXPECT_EQ(set_len(dst), 4);
    for (int i = 1; i <= 4; i++)
        EXPECT_TRUE(set_contains(dst, &i));
    growing_arena_destroy(a);
}

TEST(set, intersection_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    /* s1 = {1,2,3,4}, s2 = {3,4,5,6} => intersection = {3,4} */
    for (int i = 1; i <= 4; i++) set_add(s1, &i);
    for (int i = 3; i <= 6; i++) set_add(s2, &i);
    EXPECT_EQ(set_intersection(dst, s1, s2), SEQC_OK);
    EXPECT_EQ(set_len(dst), 2);
    int three = 3, four = 4, one = 1, five = 5;
    EXPECT_TRUE(set_contains(dst, &three));
    EXPECT_TRUE(set_contains(dst, &four));
    EXPECT_FALSE(set_contains(dst, &one));
    EXPECT_FALSE(set_contains(dst, &five));
    growing_arena_destroy(a);
}

TEST(set, intersection_empty_result)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++) set_add(s1, &i);
    for (int i = 10; i < 15; i++) set_add(s2, &i);
    EXPECT_EQ(set_intersection(dst, s1, s2), SEQC_OK);
    EXPECT_EQ(set_len(dst), 0);
    growing_arena_destroy(a);
}

TEST(set, difference_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    /* s1 = {1,2,3,4}, s2 = {3,4,5} => difference = {1,2} */
    for (int i = 1; i <= 4; i++) set_add(s1, &i);
    for (int i = 3; i <= 5; i++) set_add(s2, &i);
    EXPECT_EQ(set_difference(dst, s1, s2), SEQC_OK);
    EXPECT_EQ(set_len(dst), 2);
    int one = 1, two = 2, three = 3;
    EXPECT_TRUE(set_contains(dst, &one));
    EXPECT_TRUE(set_contains(dst, &two));
    EXPECT_FALSE(set_contains(dst, &three));
    growing_arena_destroy(a);
}

TEST(set, difference_empty_when_subset)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    /* s1 ⊆ s2 => difference is empty */
    for (int i = 0; i < 3; i++) set_add(s1, &i);
    for (int i = 0; i < 10; i++) set_add(s2, &i);
    EXPECT_EQ(set_difference(dst, s1, s2), SEQC_OK);
    EXPECT_EQ(set_len(dst), 0);
    growing_arena_destroy(a);
}

/* ---- set algebra edge cases -------------------------------------------- */

TEST(set, union_with_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *empty = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++) set_add(s, &i);
    EXPECT_EQ(set_union(dst, s, empty), SEQC_OK);
    EXPECT_EQ(set_len(dst), 5);
    for (int i = 0; i < 5; i++)
        EXPECT_TRUE(set_contains(dst, &i));
    growing_arena_destroy(a);
}

TEST(set, intersection_with_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *empty = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++) set_add(s, &i);
    EXPECT_EQ(set_intersection(dst, s, empty), SEQC_OK);
    EXPECT_EQ(set_len(dst), 0);
    growing_arena_destroy(a);
}

TEST(set, difference_with_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *empty = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++) set_add(s, &i);
    /* s \ {} == s */
    EXPECT_EQ(set_difference(dst, s, empty), SEQC_OK);
    EXPECT_EQ(set_len(dst), 5);
    for (int i = 0; i < 5; i++)
        EXPECT_TRUE(set_contains(dst, &i));
    growing_arena_destroy(a);
}

TEST(set, difference_self_is_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++) set_add(s, &i);
    /* s \ s == {} */
    EXPECT_EQ(set_difference(dst, s, s), SEQC_OK);
    EXPECT_EQ(set_len(dst), 0);
    growing_arena_destroy(a);
}

TEST(set, intersection_self_equals_self)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++) set_add(s, &i);
    /* s ∩ s == s */
    EXPECT_EQ(set_intersection(dst, s, s), SEQC_OK);
    EXPECT_EQ(set_len(dst), 5);
    for (int i = 0; i < 5; i++)
        EXPECT_TRUE(set_contains(dst, &i));
    growing_arena_destroy(a);
}

TEST(set, union_both_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    EXPECT_EQ(set_union(dst, s1, s2), SEQC_OK);
    EXPECT_EQ(set_len(dst), 0);
    growing_arena_destroy(a);
}

/* ---- OOM paths --------------------------------------------------------- */

TEST(set, create_returns_null_on_oom)
{
    Set *s = set_create(sizeof(int), int_hash, int_eq, null_allocator());
    EXPECT_EQ(s, nullptr);
}

TEST(set, add_returns_oom_when_bucket_alloc_fails)
{
    /* alloc #1 (Set struct) ok; alloc #2 (bucket array on first add) fails */
    OomCtx ctx;
    allocator_t al = oom_after_allocator(1, &ctx);
    Set *s = set_create(sizeof(int), int_hash, int_eq, al);
    EXPECT_NE(s, nullptr);
    int v = 1;
    EXPECT_EQ(set_add(s, &v), SEQC_OOM);
    EXPECT_EQ(set_len(s), 0);
    set_free(s); /* oom_free ignores remaining count, so this is safe */
}

TEST(set, add_returns_oom_when_key_alloc_fails)
{
    /* alloc #1: Set struct; alloc #2: bucket array; alloc #3: key copy fails */
    OomCtx ctx;
    allocator_t al = oom_after_allocator(2, &ctx);
    Set *s = set_create(sizeof(int), int_hash, int_eq, al);
    EXPECT_NE(s, nullptr);
    int v = 1;
    EXPECT_EQ(set_add(s, &v), SEQC_OOM);
    EXPECT_EQ(set_len(s), 0);
    set_free(s); /* oom_free ignores remaining count, so this is safe */
}
