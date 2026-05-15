#include <gtest/gtest.h>

extern "C"
{
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/hash.h"
#include "seqc/hashmap.h"
#include "seqc/iter.h"
}
#include "../oom_alloc.h"

/* ---- string-keyed helpers ---- */
static HashMap *make_str_map(growing_arena_t *a)
{
    return hashmap_create(
        sizeof(char *),
        sizeof(int),
        hash_fnv1a_str,
        hash_eq_str,
        growing_arena_allocator(a));
}

TEST(hashmap, str_set_and_get)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = make_str_map(a);
    const char *k = "hello";
    int v = 123;
    hashmap_set(m, &k, &v);
    int got;
    EXPECT_EQ(hashmap_get(m, &k, &got), SEQC_OK);
    EXPECT_EQ(got, 123);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, str_distinct_keys)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = make_str_map(a);
    const char *k1 = "foo";
    const char *k2 = "bar";
    int v1 = 1, v2 = 2;
    hashmap_set(m, &k1, &v1);
    hashmap_set(m, &k2, &v2);
    int got1, got2;
    EXPECT_EQ(hashmap_get(m, &k1, &got1), SEQC_OK);
    EXPECT_EQ(got1, 1);
    EXPECT_EQ(hashmap_get(m, &k2, &got2), SEQC_OK);
    EXPECT_EQ(got2, 2);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, str_update_existing)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = make_str_map(a);
    const char *k = "key";
    int v1 = 10, v2 = 99;
    hashmap_set(m, &k, &v1);
    hashmap_set(m, &k, &v2);
    EXPECT_EQ(hashmap_len(m), 1);
    int got;
    EXPECT_EQ(hashmap_get(m, &k, &got), SEQC_OK);
    EXPECT_EQ(got, 99);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, str_delete)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = make_str_map(a);
    const char *k = "gone";
    int v = 7;
    hashmap_set(m, &k, &v);
    EXPECT_EQ(hashmap_delete(m, &k), SEQC_OK);
    EXPECT_NE(hashmap_get(m, &k, NULL), SEQC_OK);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, str_equal_content_different_pointer)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = make_str_map(a);
    /* Two different pointers to the same content must match */
    char buf1[] = "same";
    char buf2[] = "same";
    const char *k1 = buf1;
    const char *k2 = buf2;
    int v = 42;
    hashmap_set(m, &k1, &v);
    int got;
    EXPECT_EQ(hashmap_get(m, &k2, &got), SEQC_OK);
    EXPECT_EQ(got, 42);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, create_is_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    EXPECT_EQ(hashmap_len(m), 0);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, set_and_get)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    int k = 1, v = 42;
    hashmap_set(m, &k, &v);
    int got;
    EXPECT_EQ(hashmap_get(m, &k, &got), SEQC_OK);
    EXPECT_EQ(got, 42);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, get_missing_returns_null)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    int k = 99;
    EXPECT_NE(hashmap_get(m, &k, NULL), SEQC_OK);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, set_updates_existing_key)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    int k = 1, v1 = 10, v2 = 20;
    hashmap_set(m, &k, &v1);
    hashmap_set(m, &k, &v2);
    EXPECT_EQ(hashmap_len(m), 1);
    int got;
    EXPECT_EQ(hashmap_get(m, &k, &got), SEQC_OK);
    EXPECT_EQ(got, 20);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, delete_existing)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    int k = 7, v = 77;
    hashmap_set(m, &k, &v);
    EXPECT_EQ(hashmap_delete(m, &k), SEQC_OK);
    EXPECT_NE(hashmap_get(m, &k, NULL), SEQC_OK);
    EXPECT_EQ(hashmap_len(m), 0);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, delete_missing)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    int k = 5;
    EXPECT_NE(hashmap_delete(m, &k), SEQC_OK);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, delete_and_reinsert)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    int k = 3, v1 = 30, v2 = 300;
    hashmap_set(m, &k, &v1);
    hashmap_delete(m, &k);
    hashmap_set(m, &k, &v2);
    int got;
    EXPECT_EQ(hashmap_get(m, &k, &got), SEQC_OK);
    EXPECT_EQ(got, 300);
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, many_entries_triggers_resize)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 0; i < 100; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    EXPECT_EQ(hashmap_len(m), 100);
    for (int i = 0; i < 100; i++)
    {
        int got;
        EXPECT_EQ(hashmap_get(m, &i, &got), SEQC_OK);
        EXPECT_EQ(got, i * 10);
    }
    hashmap_free(m);
    growing_arena_destroy(a);
}

/* ---- iter integration -------------------------------------------------- */

static void sum_values(const void *elem, void *ctx)
{
    const HashMapEntry *e = (const HashMapEntry *)elem;
    *(int *)ctx += *(int *)e->value;
}

TEST(hashmap, iter_foreach_sums_values)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    int sum = 0;
    iter_foreach(hashmap_iter(m), sum_values, &sum);
    EXPECT_EQ(sum, 100); /* 10+20+30+40 */
    hashmap_free(m);
    growing_arena_destroy(a);
}

static bool value_gt_20(const void *elem, void *ctx)
{
    (void)ctx;
    const HashMapEntry *e = (const HashMapEntry *)elem;
    return *(int *)e->value > 20;
}

TEST(hashmap, iter_filter_entries)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    size_t count = iter_count(iter_filter(hashmap_iter(m), value_gt_20, NULL));
    EXPECT_EQ(count, 2); /* 30 and 40 */
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, delete_middle_of_cluster)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    /* Insert several keys and delete one in the middle to exercise backward
     * shift */
    for (int i = 0; i < 10; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    int mid = 5;
    hashmap_delete(m, &mid);
    EXPECT_NE(hashmap_get(m, &mid, NULL), SEQC_OK);
    for (int i = 0; i < 10; i++)
    {
        if (i == mid)
            continue;
        int got;
        EXPECT_EQ(hashmap_get(m, &i, &got), SEQC_OK);
        EXPECT_EQ(got, i);
    }
    hashmap_free(m);
    growing_arena_destroy(a);
}

/* ---- hashmap_clear ----------------------------------------------------- */

TEST(hashmap, clear_empties_map)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    hashmap_clear(m);
    EXPECT_EQ(hashmap_len(m), 0);
    for (int i = 0; i < 5; i++)
        EXPECT_NE(hashmap_get(m, &i, NULL), SEQC_OK);
    growing_arena_destroy(a);
}

TEST(hashmap, clear_allows_reuse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    hashmap_clear(m);
    int k = 99, v = 42;
    hashmap_set(m, &k, &v);
    EXPECT_EQ(hashmap_len(m), 1);
    int got;
    EXPECT_EQ(hashmap_get(m, &k, &got), SEQC_OK);
    EXPECT_EQ(got, 42);
    growing_arena_destroy(a);
}

/* ---- hashmap_iter_rev -------------------------------------------------- */

TEST(hashmap, iter_rev_visits_same_entries_in_reverse)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 1; i <= 5; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    /* Collect forward then reverse */
    HashMapEntry fwd[5], rev[5];
    size_t nf = 0, nr = 0;
    Iter it_fwd = hashmap_iter(m);
    while (it_fwd.next(&it_fwd, &fwd[nf]))
        nf++;
    iter_drop(&it_fwd);
    Iter it_rev = hashmap_iter_rev(m);
    while (it_rev.next(&it_rev, &rev[nr]))
        nr++;
    iter_drop(&it_rev);
    EXPECT_EQ(nf, 5);
    EXPECT_EQ(nr, 5);
    /* rev must be exactly the reverse of fwd */
    for (size_t i = 0; i < 5; i++)
    {
        EXPECT_EQ(fwd[i].key, rev[4 - i].key);
        EXPECT_EQ(fwd[i].value, rev[4 - i].value);
    }
    growing_arena_destroy(a);
}

TEST(hashmap, iter_rev_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    Iter it = hashmap_iter_rev(m);
    HashMapEntry e;
    EXPECT_TRUE(!it.next(&it, &e));
    iter_drop(&it);
    hashmap_free(m);
    growing_arena_destroy(a);
}

/* ---- hashmap_contains --------------------------------------------------- */

TEST(hashmap, contains_returns_true_for_existing_key)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    int k = 42, v = 99;
    hashmap_set(m, &k, &v);
    EXPECT_TRUE(hashmap_contains(m, &k));
    hashmap_free(m);
    growing_arena_destroy(a);
}

TEST(hashmap, contains_returns_false_for_missing_key)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    int k = 42;
    EXPECT_TRUE(!hashmap_contains(m, &k));
    hashmap_free(m);
    growing_arena_destroy(a);
}

/* ---- collision / probe-chain tests ------------------------------------- */

/* Hash that always maps to slot 0 — forces maximum probing. */
static size_t always_zero_hash(const void *key, size_t key_size)
{
    (void)key;
    (void)key_size;
    return 0;
}

/*
 * Hash that maps keys 1,4 → slot 0 and keys 2,3 → their face-value slots.
 * Inserting in order 1,2,3,4 triggers Robin Hood displacement:
 *   - key4 (home=0) probes to slot 1 where key2 sits with PSL=1 < key4's
 *     PSL=2, so key4 steals the slot and displaces key2/key3 rightward.
 */
static size_t robin_hood_hash(const void *key, size_t key_size)
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
        return 0; /* same home as key 1 */
    default:
        return (size_t)(unsigned)(*(const int *)key);
    }
}

/*
 * All keys land on slot 0.
 * Exercises the probe loop (psl++, slot++) in both hashmap_set and hashmap_get.
 */
TEST(hashmap, collision_probe_set_and_get)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        always_zero_hash,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    EXPECT_EQ(hashmap_len(m), 4);
    for (int i = 1; i <= 4; i++)
    {
        int got;
        EXPECT_EQ(hashmap_get(m, &i, &got), SEQC_OK);
        EXPECT_EQ(got, i * 10);
    }
    growing_arena_destroy(a);
}

/*
 * Robin Hood displacement: insert 1,2,3 at their home slots (PSL=1 each),
 * then insert 4 (home=0). When 4 reaches slot 1 its PSL=2 > key2's PSL=1,
 * so 4 steals slot 1 and cascades, displacing key2 then key3 rightward.
 * All four keys must remain retrievable after the cascade.
 */
TEST(hashmap, robin_hood_displacement)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        robin_hood_hash,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    EXPECT_EQ(hashmap_len(m), 4);
    for (int i = 1; i <= 4; i++)
    {
        int got;
        EXPECT_EQ(hashmap_get(m, &i, &got), SEQC_OK);
        EXPECT_EQ(got, i * 10);
    }
    growing_arena_destroy(a);
}

/*
 * Delete a key that is NOT at its home slot (must probe to find it).
 * Also exercises the backward-shift loop: after deletion the chain must
 * be compacted so the remaining keys are still reachable.
 * Uses always_zero_hash so every key is displaced from home.
 */
TEST(hashmap, collision_delete_probe_and_backward_shift)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        always_zero_hash,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    /* key 3 sits at slot 2 (home=0); delete requires probing slots 0,1 first
     * and then backward-shifts key 4 into the vacated slot. */
    int k = 3;
    EXPECT_EQ(hashmap_delete(m, &k), SEQC_OK);
    EXPECT_NE(hashmap_get(m, &k, NULL), SEQC_OK);
    for (int i = 1; i <= 4; i++)
    {
        if (i == 3)
            continue;
        int got;
        EXPECT_EQ(hashmap_get(m, &i, &got), SEQC_OK);
        EXPECT_EQ(got, i * 10);
    }
    EXPECT_EQ(hashmap_len(m), 3);
    growing_arena_destroy(a);
}

/* ---- guard / edge-case tests ------------------------------------------- */

/* hash_fnv1a returns 0 for NULL key or zero key_size. */
TEST(hashmap, fnv1a_null_and_zero_size)
{
    int x = 1;
    EXPECT_EQ(hash_fnv1a(NULL, sizeof(int)), (size_t)0);
    EXPECT_EQ(hash_fnv1a(&x, 0), (size_t)0);
}

/* hash_eq_bytes: zero-size keys are always equal; NULL pointers are not. */
TEST(hashmap, eq_bytes_edge_cases)
{
    int a = 1, b = 1;
    EXPECT_TRUE(hash_eq_bytes(&a, &b, 0));
    EXPECT_TRUE(!hash_eq_bytes(NULL, &b, sizeof(int)));
    EXPECT_TRUE(!hash_eq_bytes(&a, NULL, sizeof(int)));
}

/* hashmap_create returns a zero-initialised map when args are invalid. */
TEST(hashmap, create_invalid_args_returns_zero_map)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    /* key_size == 0 */
    HashMap *m = hashmap_create(
        0, sizeof(int), hash_fnv1a, hash_eq_bytes, growing_arena_allocator(a));
    EXPECT_EQ(m, nullptr);
    growing_arena_destroy(a);
}

/* ---- sys_allocator: exercises all allocator.free branches -------------- */

/*
 * Use sys_allocator so that hashmap_free, hashmap_clear, hashmap_delete, and
 * hashmap_resize_and_rehash all exercise their allocator.free branches.
 */
TEST(hashmap, sys_alloc_free_releases_memory)
{
    allocator_t al = sys_allocator();
    HashMap *m =
        hashmap_create(sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    for (int i = 0; i < 5; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    EXPECT_EQ(hashmap_len(m), 5);
    hashmap_free(m);
    /* memory released — verified by sys_allocator not leaking */
}

TEST(hashmap, sys_alloc_clear_frees_entries)
{
    allocator_t al = sys_allocator();
    HashMap *m =
        hashmap_create(sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    for (int i = 0; i < 4; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    hashmap_clear(m);
    EXPECT_EQ(hashmap_len(m), 0);
    /* map is still usable after clear */
    int k = 99, v = 42;
    hashmap_set(m, &k, &v);
    int got;
    EXPECT_EQ(hashmap_get(m, &k, &got), SEQC_OK);
    EXPECT_EQ(got, 42);
    hashmap_free(m);
}

TEST(hashmap, sys_alloc_resize_frees_old_buckets)
{
    /* Trigger resize (capacity doubles past 75% load) so that
     * hashmap_resize_and_rehash frees the old bucket array and individual
     * keys/values through the sys allocator. */
    allocator_t al = sys_allocator();
    HashMap *m =
        hashmap_create(sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    /* Initial cap is 16; resize triggers when len > 12. */
    for (int i = 0; i < 14; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    EXPECT_EQ(hashmap_len(m), 14);
    for (int i = 0; i < 14; i++)
    {
        int got;
        EXPECT_EQ(hashmap_get(m, &i, &got), SEQC_OK);
        EXPECT_EQ(got, i);
    }
    hashmap_free(m);
}

TEST(hashmap, is_healthy_normal_load)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 8192);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 0; i < 50; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    EXPECT_TRUE(hashmap_is_healthy(m));
    growing_arena_destroy(a);
}

TEST(hashmap, audit_normal_load)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 8192);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        growing_arena_allocator(a));
    for (int i = 0; i < 50; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    HashMapStats s = hashmap_audit(m);
    EXPECT_EQ(s.len, 50);
    EXPECT_TRUE(s.cap >= 50);
    EXPECT_TRUE(s.load_factor > 0.0 && s.load_factor <= 1.0);
    EXPECT_TRUE(s.max_psl >= 1);
    EXPECT_TRUE(s.mean_psl >= 1.0);
    EXPECT_TRUE(s.is_healthy);
    growing_arena_destroy(a);
}

/* ---- OOM paths --------------------------------------------------------- */

TEST(hashmap, create_returns_null_on_oom)
{
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, null_allocator());
    EXPECT_EQ(m, nullptr);
}

TEST(hashmap, create_returns_null_when_bucket_alloc_fails)
{
    /* alloc #1 (HashMap struct) succeeds, alloc #2 (bucket array) fails */
    OomCtx ctx;
    allocator_t al = oom_after_allocator(1, &ctx);
    HashMap *m =
        hashmap_create(sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    EXPECT_EQ(m, nullptr);
    /* no leak: struct was freed by hashmap_create on bucket failure */
}

TEST(hashmap, set_returns_oom_when_key_alloc_fails)
{
    /* allocs 1-2: create (struct + buckets); alloc 3 (key copy) fails */
    OomCtx ctx;
    allocator_t al = oom_after_allocator(2, &ctx);
    HashMap *m =
        hashmap_create(sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    EXPECT_NE(m, nullptr);
    int k = 1, v = 1;
    EXPECT_EQ(hashmap_set(m, &k, &v), SEQC_OOM);
    EXPECT_EQ(hashmap_len(m), 0);
    hashmap_free(m);
}

TEST(hashmap, set_returns_oom_when_val_alloc_fails)
{
    /* allocs 1-2: create; alloc 3 (key copy) ok; alloc 4 (val copy) fails */
    OomCtx ctx;
    allocator_t al = oom_after_allocator(3, &ctx);
    HashMap *m =
        hashmap_create(sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    EXPECT_NE(m, nullptr);
    int k = 1, v = 1;
    EXPECT_EQ(hashmap_set(m, &k, &v), SEQC_OOM);
    EXPECT_EQ(hashmap_len(m), 0);
    hashmap_free(m);
}
