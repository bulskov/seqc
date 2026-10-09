#include "ctt.h"

#include "../oom_alloc.h"
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/set.h"

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

TEST(set_is_empty_on_create)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    ASSERT_EQ(0, set_len(s));
    growing_arena_destroy(a);
}

TEST(set_add_contains)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int v1 = 1, v2 = 2, v3 = 42;
    ASSERT_EQ(SEQC_OK, set_add(s, &v1));
    ASSERT_EQ(SEQC_OK, set_add(s, &v2));
    ASSERT_EQ(SEQC_OK, set_add(s, &v3));
    ASSERT_TRUE(set_contains(s, &v1));
    ASSERT_TRUE(set_contains(s, &v2));
    ASSERT_TRUE(set_contains(s, &v3));
    ASSERT_EQ(3, set_len(s));
    growing_arena_destroy(a);
}

TEST(set_add_duplicate_returns_0)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int v = 7;
    ASSERT_EQ(SEQC_OK, set_add(s, &v));
    ASSERT_NE(SEQC_OK, set_add(s, &v)); /* already present */
    ASSERT_EQ(1, set_len(s));
    growing_arena_destroy(a);
}

TEST(set_remove_existing)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int v = 5;
    set_add(s, &v);
    ASSERT_EQ(SEQC_OK, set_remove(s, &v));
    ASSERT_FALSE(set_contains(s, &v));
    ASSERT_EQ(0, set_len(s));
    growing_arena_destroy(a);
}

TEST(set_remove_nonexistent_returns_0)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int v = 99;
    ASSERT_NE(SEQC_OK, set_remove(s, &v));
    growing_arena_destroy(a);
}

TEST(set_does_not_contain_absent_key)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int present = 1, absent = 2;
    set_add(s, &present);
    ASSERT_FALSE(set_contains(s, &absent));
    growing_arena_destroy(a);
}

TEST(set_iter_yields_all_elements)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    int vals[] = {10, 20, 30, 40};
    for (int i = 0; i < 4; i++)
    {
        set_add(s, &vals[i]);
    }
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t count = iter_count(set_iter(s));
    ASSERT_EQ(4, count);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(set_grow_beyond_initial_cap)
{
    /* Add 20 elements to force a resize (load factor 0.75 of initial cap 16) */
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 20; i++)
    {
        set_add(s, &i);
    }
    ASSERT_EQ(20, set_len(s));
    for (int i = 0; i < 20; i++)
    {
        ASSERT_TRUE(set_contains(s, &i));
    }
    growing_arena_destroy(a);
}

/* ---- set_clear --------------------------------------------------------- */

TEST(set_clear_empties_set)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        set_add(s, &i);
    }
    set_clear(s);
    ASSERT_EQ(0, set_len(s));
    for (int i = 0; i < 5; i++)
    {
        ASSERT_TRUE(!set_contains(s, &i));
    }
    growing_arena_destroy(a);
}

TEST(set_clear_allows_reuse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        set_add(s, &i);
    }
    set_clear(s);
    int x = 42;
    ASSERT_EQ(SEQC_OK, set_add(s, &x));
    ASSERT_EQ(1, set_len(s));
    ASSERT_TRUE(set_contains(s, &x));
    growing_arena_destroy(a);
}

/* ---- set_iter_rev ------------------------------------------------------- */

TEST(set_iter_rev_yields_all_elements)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        set_add(s, &i);
    }
    iter_t it = set_iter_rev(s);
    size_t n = 0;
    int v;
    while (it.next(&it, &v))
    {
        n++;
    }
    iter_destroy(&it);
    ASSERT_EQ(5, n);
    growing_arena_destroy(a);
}

TEST(set_iter_rev_empty_set)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    iter_t it = set_iter_rev(s);
    int v;
    ASSERT_TRUE(!it.next(&it, &v));
    iter_destroy(&it);
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
TEST(set_collision_probe_insert_and_contains)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s = set_create(
        sizeof(int), always_zero_set_hash, int_eq, growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        ASSERT_EQ(SEQC_OK, set_add(s, &i));
    }
    ASSERT_EQ(4, set_len(s));
    for (int i = 1; i <= 4; i++)
    {
        ASSERT_TRUE(set_contains(s, &i));
    }
    growing_arena_destroy(a);
}

/*
 * Robin Hood displacement: key 4 (home=0) probes past key 1, then steals
 * slot 1 from key 2 (PSL=1 < incoming PSL=2), cascading key 2 and key 3
 * rightward.  All four elements must remain members after the cascade.
 */
TEST(set_collision_robin_hood_displacement)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s = set_create(
        sizeof(int), robin_hood_set_hash, int_eq, growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        ASSERT_EQ(SEQC_OK, set_add(s, &i));
    }
    ASSERT_EQ(4, set_len(s));
    for (int i = 1; i <= 4; i++)
    {
        ASSERT_TRUE(set_contains(s, &i));
    }
    growing_arena_destroy(a);
}

/*
 * Remove an element that is NOT at its home slot (must probe to find it),
 * then exercise the backward-shift loop (nb->psl > 1) to compact the chain.
 * The remaining elements must still be findable.
 */
TEST(set_collision_remove_probe_and_backward_shift)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s = set_create(
        sizeof(int), always_zero_set_hash, int_eq, growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        set_add(s, &i);
    }
    /* elem 3 sits at slot 2 (home=0): remove requires probing slots 0,1 first,
     * then backward-shifts elem 4 into the vacated slot. */
    int k = 3;
    ASSERT_EQ(SEQC_OK, set_remove(s, &k));
    ASSERT_TRUE(!set_contains(s, &k));
    for (int i = 1; i <= 4; i++)
    {
        if (i == 3)
        {
            continue;
        }
        ASSERT_TRUE(set_contains(s, &i));
    }
    ASSERT_EQ(3, set_len(s));
    growing_arena_destroy(a);
}

/*
 * set_remove on a non-empty set where the key is absent; exercises the
 * b->psl == 0 early-exit path (distinct from the s->len == 0 guard).
 */
TEST(set_remove_absent_hits_empty_slot)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s = set_create(
        sizeof(int), always_zero_set_hash, int_eq, growing_arena_allocator(a));
    int present = 1;
    set_add(s, &present);
    /* key 99 also hashes to slot 0 but is not in the set; after probing past
     * present we hit an empty slot and must return false. */
    int absent = 99;
    ASSERT_NE(SEQC_OK, set_remove(s, &absent));
    ASSERT_EQ(1, set_len(s));
    growing_arena_destroy(a);
}

/* ---- sys_allocator: exercises all allocator.free branches -------------- */

TEST(set_sys_alloc_destroy_releases_memory)
{
    allocator_t al = sys_allocator();
    set_t *s = set_create(sizeof(int), int_hash, int_eq, al);
    for (int i = 0; i < 5; i++)
    {
        set_add(s, &i);
    }
    ASSERT_EQ(5, set_len(s));
    set_destroy(s);
    /* memory released — verified by sys_allocator not leaking */
}

TEST(set_sys_alloc_empty_destroy_releases_struct)
{
    allocator_t al = sys_allocator();
    set_t *s = set_create(sizeof(int), int_hash, int_eq, al);
    ASSERT_NOT_NULL(s);
    /* No adds: the bucket array is never allocated, but set_destroy must still
     * release the set struct itself (else it leaks). */
    set_destroy(s);
}

TEST(set_sys_alloc_clear_frees_keys)
{
    allocator_t al = sys_allocator();
    set_t *s = set_create(sizeof(int), int_hash, int_eq, al);
    for (int i = 0; i < 4; i++)
    {
        set_add(s, &i);
    }
    set_clear(s);
    ASSERT_EQ(0, set_len(s));
    /* set is still usable after clear */
    int x = 42;
    ASSERT_EQ(SEQC_OK, set_add(s, &x));
    ASSERT_TRUE(set_contains(s, &x));
    set_destroy(s);
}

TEST(set_sys_alloc_remove_frees_key)
{
    allocator_t al = sys_allocator();
    set_t *s = set_create(sizeof(int), int_hash, int_eq, al);
    int v = 7;
    set_add(s, &v);
    ASSERT_EQ(SEQC_OK, set_remove(s, &v));
    ASSERT_TRUE(!set_contains(s, &v));
    set_destroy(s);
}

TEST(set_is_healthy_normal_load)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 50; i++)
    {
        set_add(s, &i);
    }
    ASSERT_TRUE(set_is_healthy(s));
    growing_arena_destroy(a);
}

TEST(set_audit_normal_load)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 50; i++)
    {
        set_add(s, &i);
    }
    set_stats_t st = set_audit(s);
    ASSERT_EQ(50, st.len);
    ASSERT_TRUE(st.cap >= 50);
    ASSERT_TRUE(st.load_factor > 0.0 && st.load_factor <= 1.0);
    ASSERT_TRUE(st.max_psl >= 1);
    ASSERT_TRUE(st.mean_psl >= 1.0);
    ASSERT_TRUE(st.is_healthy);
    growing_arena_destroy(a);
}

/* ---- set algebra ------------------------------------------------------- */

TEST(set_union_disjoint)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s1 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *s2 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        set_add(s1, &i);
    }
    for (int i = 5; i < 10; i++)
    {
        set_add(s2, &i);
    }
    ASSERT_EQ(SEQC_OK, set_union(dst, s1, s2));
    ASSERT_EQ(10, set_len(dst));
    for (int i = 0; i < 10; i++)
    {
        ASSERT_TRUE(set_contains(dst, &i));
    }
    growing_arena_destroy(a);
}

TEST(set_union_overlapping)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s1 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *s2 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    /* s1 = {1,2,3}, s2 = {2,3,4} => union = {1,2,3,4} */
    int v[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
    {
        set_add(s1, &v[i]);
    }
    int v2[] = {2, 3, 4};
    for (int i = 0; i < 3; i++)
    {
        set_add(s2, &v2[i]);
    }
    ASSERT_EQ(SEQC_OK, set_union(dst, s1, s2));
    ASSERT_EQ(4, set_len(dst));
    for (int i = 1; i <= 4; i++)
    {
        ASSERT_TRUE(set_contains(dst, &i));
    }
    growing_arena_destroy(a);
}

TEST(set_intersection_basic)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s1 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *s2 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    /* s1 = {1,2,3,4}, s2 = {3,4,5,6} => intersection = {3,4} */
    for (int i = 1; i <= 4; i++)
    {
        set_add(s1, &i);
    }
    for (int i = 3; i <= 6; i++)
    {
        set_add(s2, &i);
    }
    ASSERT_EQ(SEQC_OK, set_intersection(dst, s1, s2));
    ASSERT_EQ(2, set_len(dst));
    int three = 3, four = 4, one = 1, five = 5;
    ASSERT_TRUE(set_contains(dst, &three));
    ASSERT_TRUE(set_contains(dst, &four));
    ASSERT_FALSE(set_contains(dst, &one));
    ASSERT_FALSE(set_contains(dst, &five));
    growing_arena_destroy(a);
}

TEST(set_intersection_empty_result)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s1 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *s2 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        set_add(s1, &i);
    }
    for (int i = 10; i < 15; i++)
    {
        set_add(s2, &i);
    }
    ASSERT_EQ(SEQC_OK, set_intersection(dst, s1, s2));
    ASSERT_EQ(0, set_len(dst));
    growing_arena_destroy(a);
}

TEST(set_difference_basic)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s1 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *s2 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    /* s1 = {1,2,3,4}, s2 = {3,4,5} => difference = {1,2} */
    for (int i = 1; i <= 4; i++)
    {
        set_add(s1, &i);
    }
    for (int i = 3; i <= 5; i++)
    {
        set_add(s2, &i);
    }
    ASSERT_EQ(SEQC_OK, set_difference(dst, s1, s2));
    ASSERT_EQ(2, set_len(dst));
    int one = 1, two = 2, three = 3;
    ASSERT_TRUE(set_contains(dst, &one));
    ASSERT_TRUE(set_contains(dst, &two));
    ASSERT_FALSE(set_contains(dst, &three));
    growing_arena_destroy(a);
}

TEST(set_difference_empty_when_subset)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s1 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *s2 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    /* s1 ⊆ s2 => difference is empty */
    for (int i = 0; i < 3; i++)
    {
        set_add(s1, &i);
    }
    for (int i = 0; i < 10; i++)
    {
        set_add(s2, &i);
    }
    ASSERT_EQ(SEQC_OK, set_difference(dst, s1, s2));
    ASSERT_EQ(0, set_len(dst));
    growing_arena_destroy(a);
}

/* ---- set algebra edge cases -------------------------------------------- */

TEST(set_union_with_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *empty =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        set_add(s, &i);
    }
    ASSERT_EQ(SEQC_OK, set_union(dst, s, empty));
    ASSERT_EQ(5, set_len(dst));
    for (int i = 0; i < 5; i++)
    {
        ASSERT_TRUE(set_contains(dst, &i));
    }
    growing_arena_destroy(a);
}

TEST(set_intersection_with_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *empty =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        set_add(s, &i);
    }
    ASSERT_EQ(SEQC_OK, set_intersection(dst, s, empty));
    ASSERT_EQ(0, set_len(dst));
    growing_arena_destroy(a);
}

TEST(set_difference_with_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *empty =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        set_add(s, &i);
    }
    /* s \ {} == s */
    ASSERT_EQ(SEQC_OK, set_difference(dst, s, empty));
    ASSERT_EQ(5, set_len(dst));
    for (int i = 0; i < 5; i++)
    {
        ASSERT_TRUE(set_contains(dst, &i));
    }
    growing_arena_destroy(a);
}

TEST(set_difference_self_is_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        set_add(s, &i);
    }
    /* s \ s == {} */
    ASSERT_EQ(SEQC_OK, set_difference(dst, s, s));
    ASSERT_EQ(0, set_len(dst));
    growing_arena_destroy(a);
}

TEST(set_intersection_self_equals_self)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    set_t *s =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        set_add(s, &i);
    }
    /* s ∩ s == s */
    ASSERT_EQ(SEQC_OK, set_intersection(dst, s, s));
    ASSERT_EQ(5, set_len(dst));
    for (int i = 0; i < 5; i++)
    {
        ASSERT_TRUE(set_contains(dst, &i));
    }
    growing_arena_destroy(a);
}

TEST(set_union_both_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    set_t *s1 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *s2 =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    set_t *dst =
        set_create(sizeof(int), int_hash, int_eq, growing_arena_allocator(a));
    ASSERT_EQ(SEQC_OK, set_union(dst, s1, s2));
    ASSERT_EQ(0, set_len(dst));
    growing_arena_destroy(a);
}

/* ---- OOM paths --------------------------------------------------------- */

TEST(set_create_returns_null_on_oom)
{
    set_t *s = set_create(sizeof(int), int_hash, int_eq, null_allocator());
    ASSERT_NULL(s);
}

TEST(set_add_returns_oom_when_bucket_alloc_fails)
{
    /* alloc #1 (set_t struct) ok; alloc #2 (bucket array on first add) fails */
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(1, &ctx);
    set_t *s = set_create(sizeof(int), int_hash, int_eq, al);
    ASSERT_NOT_NULL(s);
    int v = 1;
    ASSERT_EQ(SEQC_OOM, set_add(s, &v));
    ASSERT_EQ(0, set_len(s));
    set_destroy(s); /* oom_free ignores remaining count, so this is safe */
}

TEST(set_add_returns_oom_when_key_alloc_fails)
{
    /* alloc #1: set_t struct; alloc #2: bucket array; alloc #3: key copy fails
     */
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(2, &ctx);
    set_t *s = set_create(sizeof(int), int_hash, int_eq, al);
    ASSERT_NOT_NULL(s);
    int v = 1;
    ASSERT_EQ(SEQC_OOM, set_add(s, &v));
    ASSERT_EQ(0, set_len(s));
    set_destroy(s); /* oom_free ignores remaining count, so this is safe */
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "set_test");
}
