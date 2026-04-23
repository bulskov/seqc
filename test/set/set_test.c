#include <criterion/criterion.h>

#include "arena/arena.h"
#include "set/set.h"

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

Test(set, is_empty_on_create)
{
    Arena *a = arena_create(256);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    cr_assert_eq(set_len(s), 0);
    arena_free(a);
}

Test(set, add_contains)
{
    Arena *a = arena_create(1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    int v1 = 1, v2 = 2, v3 = 42;
    cr_assert_eq(set_add(s, &v1), SEQC_OK);
    cr_assert_eq(set_add(s, &v2), SEQC_OK);
    cr_assert_eq(set_add(s, &v3), SEQC_OK);
    cr_assert(set_contains(s, &v1));
    cr_assert(set_contains(s, &v2));
    cr_assert(set_contains(s, &v3));
    cr_assert_eq(set_len(s), 3);
    arena_free(a);
}

Test(set, add_duplicate_returns_0)
{
    Arena *a = arena_create(512);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    int v = 7;
    cr_assert_eq(set_add(s, &v), SEQC_OK);
    cr_assert_neq(set_add(s, &v), SEQC_OK); /* already present */
    cr_assert_eq(set_len(s), 1);
    arena_free(a);
}

Test(set, remove_existing)
{
    Arena *a = arena_create(512);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    int v = 5;
    set_add(s, &v);
    cr_assert_eq(set_remove(s, &v), SEQC_OK);
    cr_assert_not(set_contains(s, &v));
    cr_assert_eq(set_len(s), 0);
    arena_free(a);
}

Test(set, remove_nonexistent_returns_0)
{
    Arena *a = arena_create(256);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    int v = 99;
    cr_assert_neq(set_remove(s, &v), SEQC_OK);
    arena_free(a);
}

Test(set, does_not_contain_absent_key)
{
    Arena *a = arena_create(512);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    int present = 1, absent = 2;
    set_add(s, &present);
    cr_assert_not(set_contains(s, &absent));
    arena_free(a);
}

Test(set, iter_yields_all_elements)
{
    Arena *a = arena_create(1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    int vals[] = {10, 20, 30, 40};
    for (int i = 0; i < 4; i++)
        set_add(s, &vals[i]);
    Scratch sc = arena_scratch_push(a);
    size_t count = iter_count(set_iter(s));
    cr_assert_eq(count, 4);
    arena_scratch_pop(&sc);
    arena_free(a);
}

Test(set, grow_beyond_initial_cap)
{
    /* Add 20 elements to force a resize (load factor 0.75 of initial cap 16) */
    Arena *a = arena_create(4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    for (int i = 0; i < 20; i++)
        set_add(s, &i);
    cr_assert_eq(set_len(s), 20);
    for (int i = 0; i < 20; i++)
        cr_assert(set_contains(s, &i));
    arena_free(a);
}

/* ---- set_clear --------------------------------------------------------- */

Test(set, clear_empties_set)
{
    Arena *a = arena_create(1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    for (int i = 0; i < 5; i++)
        set_add(s, &i);
    set_clear(s);
    cr_assert_eq(set_len(s), 0);
    for (int i = 0; i < 5; i++)
        cr_assert(!set_contains(s, &i));
    arena_free(a);
}

Test(set, clear_allows_reuse)
{
    Arena *a = arena_create(1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    for (int i = 0; i < 3; i++)
        set_add(s, &i);
    set_clear(s);
    int x = 42;
    cr_assert_eq(set_add(s, &x), SEQC_OK);
    cr_assert_eq(set_len(s), 1);
    cr_assert(set_contains(s, &x));
    arena_free(a);
}

/* ---- set_iter_rev ------------------------------------------------------- */

Test(set, iter_rev_yields_all_elements)
{
    Arena *a = arena_create(1024);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    for (int i = 0; i < 5; i++)
        set_add(s, &i);
    Iter it = set_iter_rev(s);
    size_t n = 0;
    int v;
    while (it.next(&it, &v))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 5);
    arena_free(a);
}

Test(set, iter_rev_empty_set)
{
    Arena *a = arena_create(256);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Iter it = set_iter_rev(s);
    int v;
    cr_assert(!it.next(&it, &v));
    iter_drop(&it);
    arena_free(a);
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
Test(set, collision_probe_insert_and_contains)
{
    Arena *a = arena_create(4096);
    Set *s = set_create(
        sizeof(int), always_zero_set_hash, int_eq, arena_allocator(a));
    for (int i = 1; i <= 4; i++)
        cr_assert_eq(set_add(s, &i), SEQC_OK);
    cr_assert_eq(set_len(s), 4);
    for (int i = 1; i <= 4; i++)
        cr_assert(set_contains(s, &i));
    arena_free(a);
}

/*
 * Robin Hood displacement: key 4 (home=0) probes past key 1, then steals
 * slot 1 from key 2 (PSL=1 < incoming PSL=2), cascading key 2 and key 3
 * rightward.  All four elements must remain members after the cascade.
 */
Test(set, collision_robin_hood_displacement)
{
    Arena *a = arena_create(4096);
    Set *s = set_create(
        sizeof(int), robin_hood_set_hash, int_eq, arena_allocator(a));
    for (int i = 1; i <= 4; i++)
        cr_assert_eq(set_add(s, &i), SEQC_OK);
    cr_assert_eq(set_len(s), 4);
    for (int i = 1; i <= 4; i++)
        cr_assert(set_contains(s, &i));
    arena_free(a);
}

/*
 * Remove an element that is NOT at its home slot (must probe to find it),
 * then exercise the backward-shift loop (nb->psl > 1) to compact the chain.
 * The remaining elements must still be findable.
 */
Test(set, collision_remove_probe_and_backward_shift)
{
    Arena *a = arena_create(4096);
    Set *s = set_create(
        sizeof(int), always_zero_set_hash, int_eq, arena_allocator(a));
    for (int i = 1; i <= 4; i++)
        set_add(s, &i);
    /* elem 3 sits at slot 2 (home=0): remove requires probing slots 0,1 first,
     * then backward-shifts elem 4 into the vacated slot. */
    int k = 3;
    cr_assert_eq(set_remove(s, &k), SEQC_OK);
    cr_assert(!set_contains(s, &k));
    for (int i = 1; i <= 4; i++)
    {
        if (i == 3)
            continue;
        cr_assert(set_contains(s, &i));
    }
    cr_assert_eq(set_len(s), 3);
    arena_free(a);
}

/*
 * set_remove on a non-empty set where the key is absent; exercises the
 * b->psl == 0 early-exit path (distinct from the s->len == 0 guard).
 */
Test(set, remove_absent_hits_empty_slot)
{
    Arena *a = arena_create(4096);
    Set *s = set_create(
        sizeof(int), always_zero_set_hash, int_eq, arena_allocator(a));
    int present = 1;
    set_add(s, &present);
    /* key 99 also hashes to slot 0 but is not in the set; after probing past
     * present we hit an empty slot and must return false. */
    int absent = 99;
    cr_assert_neq(set_remove(s, &absent), SEQC_OK);
    cr_assert_eq(set_len(s), 1);
    arena_free(a);
}

/* ---- sys_allocator: exercises all allocator.free branches -------------- */

Test(set, sys_alloc_free_releases_memory)
{
    Allocator al = sys_allocator();
    Set *s = set_create(sizeof(int), int_hash, int_eq, al);
    for (int i = 0; i < 5; i++)
        set_add(s, &i);
    cr_assert_eq(set_len(s), 5);
    set_free(s);
    /* memory released — verified by sys_allocator not leaking */
}

Test(set, sys_alloc_clear_frees_keys)
{
    Allocator al = sys_allocator();
    Set *s = set_create(sizeof(int), int_hash, int_eq, al);
    for (int i = 0; i < 4; i++)
        set_add(s, &i);
    set_clear(s);
    cr_assert_eq(set_len(s), 0);
    /* set is still usable after clear */
    int x = 42;
    cr_assert_eq(set_add(s, &x), SEQC_OK);
    cr_assert(set_contains(s, &x));
    set_free(s);
}

Test(set, sys_alloc_remove_frees_key)
{
    Allocator al = sys_allocator();
    Set *s = set_create(sizeof(int), int_hash, int_eq, al);
    int v = 7;
    set_add(s, &v);
    cr_assert_eq(set_remove(s, &v), SEQC_OK);
    cr_assert(!set_contains(s, &v));
    set_free(s);
}

Test(set, is_healthy_normal_load)
{
    Arena *a = arena_create(4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    for (int i = 0; i < 50; i++)
        set_add(s, &i);
    cr_assert(set_is_healthy(s));
    arena_free(a);
}

Test(set, audit_normal_load)
{
    Arena *a = arena_create(4096);
    Set *s = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    for (int i = 0; i < 50; i++)
        set_add(s, &i);
    SetStats st = set_audit(s);
    cr_assert_eq(st.len, 50);
    cr_assert(st.cap >= 50);
    cr_assert(st.load_factor > 0.0 && st.load_factor <= 1.0);
    cr_assert(st.max_psl >= 1);
    cr_assert(st.mean_psl >= 1.0);
    cr_assert(st.is_healthy);
    arena_free(a);
}

/* ---- set algebra ------------------------------------------------------- */

Test(set, union_disjoint)
{
    Arena *a = arena_create(4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    for (int i = 0; i < 5; i++) set_add(s1, &i);
    for (int i = 5; i < 10; i++) set_add(s2, &i);
    cr_assert_eq(set_union(dst, s1, s2), SEQC_OK);
    cr_assert_eq(set_len(dst), 10);
    for (int i = 0; i < 10; i++)
        cr_assert(set_contains(dst, &i));
    arena_free(a);
}

Test(set, union_overlapping)
{
    Arena *a = arena_create(4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    /* s1 = {1,2,3}, s2 = {2,3,4} => union = {1,2,3,4} */
    int v[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) set_add(s1, &v[i]);
    int v2[] = {2, 3, 4};
    for (int i = 0; i < 3; i++) set_add(s2, &v2[i]);
    cr_assert_eq(set_union(dst, s1, s2), SEQC_OK);
    cr_assert_eq(set_len(dst), 4);
    for (int i = 1; i <= 4; i++)
        cr_assert(set_contains(dst, &i));
    arena_free(a);
}

Test(set, intersection_basic)
{
    Arena *a = arena_create(4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    /* s1 = {1,2,3,4}, s2 = {3,4,5,6} => intersection = {3,4} */
    for (int i = 1; i <= 4; i++) set_add(s1, &i);
    for (int i = 3; i <= 6; i++) set_add(s2, &i);
    cr_assert_eq(set_intersection(dst, s1, s2), SEQC_OK);
    cr_assert_eq(set_len(dst), 2);
    int three = 3, four = 4, one = 1, five = 5;
    cr_assert(set_contains(dst, &three));
    cr_assert(set_contains(dst, &four));
    cr_assert_not(set_contains(dst, &one));
    cr_assert_not(set_contains(dst, &five));
    arena_free(a);
}

Test(set, intersection_empty_result)
{
    Arena *a = arena_create(4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    for (int i = 0; i < 5; i++) set_add(s1, &i);
    for (int i = 10; i < 15; i++) set_add(s2, &i);
    cr_assert_eq(set_intersection(dst, s1, s2), SEQC_OK);
    cr_assert_eq(set_len(dst), 0);
    arena_free(a);
}

Test(set, difference_basic)
{
    Arena *a = arena_create(4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    /* s1 = {1,2,3,4}, s2 = {3,4,5} => difference = {1,2} */
    for (int i = 1; i <= 4; i++) set_add(s1, &i);
    for (int i = 3; i <= 5; i++) set_add(s2, &i);
    cr_assert_eq(set_difference(dst, s1, s2), SEQC_OK);
    cr_assert_eq(set_len(dst), 2);
    int one = 1, two = 2, three = 3;
    cr_assert(set_contains(dst, &one));
    cr_assert(set_contains(dst, &two));
    cr_assert_not(set_contains(dst, &three));
    arena_free(a);
}

Test(set, difference_empty_when_subset)
{
    Arena *a = arena_create(4096);
    Set *s1 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *s2 = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    Set *dst = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
    /* s1 ⊆ s2 => difference is empty */
    for (int i = 0; i < 3; i++) set_add(s1, &i);
    for (int i = 0; i < 10; i++) set_add(s2, &i);
    cr_assert_eq(set_difference(dst, s1, s2), SEQC_OK);
    cr_assert_eq(set_len(dst), 0);
    arena_free(a);
}
