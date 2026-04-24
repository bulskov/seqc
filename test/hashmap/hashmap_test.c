#include <criterion/criterion.h>

#include "seqc/arena.h"
#include "seqc/hashmap.h"
#include "seqc/iter.h"
#include "seqc/hash.h"
#include "../oom_alloc.h"

/* ---- string-keyed helpers ---- */
static HashMap *make_str_map(Arena *a)
{
    return hashmap_create(
        sizeof(char *),
        sizeof(int),
        hash_fnv1a_str,
        hash_eq_str,
        arena_allocator(a));
}

Test(hashmap, str_set_and_get)
{
    Arena *a = arena_create(4096);
    HashMap *m = make_str_map(a);
    const char *k = "hello";
    int v = 123;
    hashmap_set(m, &k, &v);
    int got;
    cr_assert_eq(hashmap_get(m, &k, &got), SEQC_OK);
    cr_assert_eq(got, 123);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, str_distinct_keys)
{
    Arena *a = arena_create(4096);
    HashMap *m = make_str_map(a);
    const char *k1 = "foo";
    const char *k2 = "bar";
    int v1 = 1, v2 = 2;
    hashmap_set(m, &k1, &v1);
    hashmap_set(m, &k2, &v2);
    int got1, got2;
    cr_assert_eq(hashmap_get(m, &k1, &got1), SEQC_OK);
    cr_assert_eq(got1, 1);
    cr_assert_eq(hashmap_get(m, &k2, &got2), SEQC_OK);
    cr_assert_eq(got2, 2);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, str_update_existing)
{
    Arena *a = arena_create(4096);
    HashMap *m = make_str_map(a);
    const char *k = "key";
    int v1 = 10, v2 = 99;
    hashmap_set(m, &k, &v1);
    hashmap_set(m, &k, &v2);
    cr_assert_eq(hashmap_len(m), 1);
    int got;
    cr_assert_eq(hashmap_get(m, &k, &got), SEQC_OK);
    cr_assert_eq(got, 99);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, str_delete)
{
    Arena *a = arena_create(4096);
    HashMap *m = make_str_map(a);
    const char *k = "gone";
    int v = 7;
    hashmap_set(m, &k, &v);
    cr_assert_eq(hashmap_delete(m, &k), SEQC_OK);
    cr_assert_neq(hashmap_get(m, &k, NULL), SEQC_OK);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, str_equal_content_different_pointer)
{
    Arena *a = arena_create(4096);
    HashMap *m = make_str_map(a);
    /* Two different pointers to the same content must match */
    char buf1[] = "same";
    char buf2[] = "same";
    const char *k1 = buf1;
    const char *k2 = buf2;
    int v = 42;
    hashmap_set(m, &k1, &v);
    int got;
    cr_assert_eq(hashmap_get(m, &k2, &got), SEQC_OK);
    cr_assert_eq(got, 42);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, create_is_empty)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    cr_assert_eq(hashmap_len(m), 0);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, set_and_get)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    int k = 1, v = 42;
    hashmap_set(m, &k, &v);
    int got;
    cr_assert_eq(hashmap_get(m, &k, &got), SEQC_OK);
    cr_assert_eq(got, 42);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, get_missing_returns_null)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    int k = 99;
    cr_assert_neq(hashmap_get(m, &k, NULL), SEQC_OK);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, set_updates_existing_key)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    int k = 1, v1 = 10, v2 = 20;
    hashmap_set(m, &k, &v1);
    hashmap_set(m, &k, &v2);
    cr_assert_eq(hashmap_len(m), 1);
    int got;
    cr_assert_eq(hashmap_get(m, &k, &got), SEQC_OK);
    cr_assert_eq(got, 20);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, delete_existing)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    int k = 7, v = 77;
    hashmap_set(m, &k, &v);
    cr_assert_eq(hashmap_delete(m, &k), SEQC_OK);
    cr_assert_neq(hashmap_get(m, &k, NULL), SEQC_OK);
    cr_assert_eq(hashmap_len(m), 0);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, delete_missing)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    int k = 5;
    cr_assert_neq(hashmap_delete(m, &k), SEQC_OK);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, delete_and_reinsert)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    int k = 3, v1 = 30, v2 = 300;
    hashmap_set(m, &k, &v1);
    hashmap_delete(m, &k);
    hashmap_set(m, &k, &v2);
    int got;
    cr_assert_eq(hashmap_get(m, &k, &got), SEQC_OK);
    cr_assert_eq(got, 300);
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, many_entries_triggers_resize)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    for (int i = 0; i < 100; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    cr_assert_eq(hashmap_len(m), 100);
    for (int i = 0; i < 100; i++)
    {
        int got;
        cr_assert_eq(hashmap_get(m, &i, &got), SEQC_OK);
        cr_assert_eq(got, i * 10);
    }
    hashmap_free(m);
    arena_free(a);
}

/* ---- iter integration -------------------------------------------------- */

static void sum_values(const void *elem, void *ctx)
{
    const HashMapEntry *e = elem;
    *(int *)ctx += *(int *)e->value;
}

Test(hashmap, iter_foreach_sums_values)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    int sum = 0;
    iter_foreach(hashmap_iter(m), sum_values, &sum);
    cr_assert_eq(sum, 100); /* 10+20+30+40 */
    hashmap_free(m);
    arena_free(a);
}

static bool value_gt_20(const void *elem, void *ctx)
{
    (void)ctx;
    const HashMapEntry *e = elem;
    return *(int *)e->value > 20;
}

Test(hashmap, iter_filter_entries)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    size_t count = iter_count(iter_filter(hashmap_iter(m), value_gt_20, NULL));
    cr_assert_eq(count, 2); /* 30 and 40 */
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, delete_middle_of_cluster)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    /* Insert several keys and delete one in the middle to exercise backward
     * shift */
    for (int i = 0; i < 10; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    int mid = 5;
    hashmap_delete(m, &mid);
    cr_assert_neq(hashmap_get(m, &mid, NULL), SEQC_OK);
    for (int i = 0; i < 10; i++)
    {
        if (i == mid)
            continue;
        int got;
        cr_assert_eq(hashmap_get(m, &i, &got), SEQC_OK);
        cr_assert_eq(got, i);
    }
    hashmap_free(m);
    arena_free(a);
}

/* ---- hashmap_clear ----------------------------------------------------- */

Test(hashmap, clear_empties_map)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    for (int i = 0; i < 5; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    hashmap_clear(m);
    cr_assert_eq(hashmap_len(m), 0);
    for (int i = 0; i < 5; i++)
        cr_assert_neq(hashmap_get(m, &i, NULL), SEQC_OK);
    arena_free(a);
}

Test(hashmap, clear_allows_reuse)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    for (int i = 0; i < 3; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    hashmap_clear(m);
    int k = 99, v = 42;
    hashmap_set(m, &k, &v);
    cr_assert_eq(hashmap_len(m), 1);
    int got;
    cr_assert_eq(hashmap_get(m, &k, &got), SEQC_OK);
    cr_assert_eq(got, 42);
    arena_free(a);
}

/* ---- hashmap_iter_rev -------------------------------------------------- */

Test(hashmap, iter_rev_visits_same_entries_in_reverse)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
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
    cr_assert_eq(nf, 5);
    cr_assert_eq(nr, 5);
    /* rev must be exactly the reverse of fwd */
    for (size_t i = 0; i < 5; i++)
    {
        cr_assert_eq(fwd[i].key, rev[4 - i].key);
        cr_assert_eq(fwd[i].value, rev[4 - i].value);
    }
    arena_free(a);
}

Test(hashmap, iter_rev_empty)
{
    Arena *a = arena_create(256);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    Iter it = hashmap_iter_rev(m);
    HashMapEntry e;
    cr_assert(!it.next(&it, &e));
    iter_drop(&it);
    hashmap_free(m);
    arena_free(a);
}

/* ---- hashmap_contains --------------------------------------------------- */

Test(hashmap, contains_returns_true_for_existing_key)
{
    Arena *a = arena_create(256);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    int k = 42, v = 99;
    hashmap_set(m, &k, &v);
    cr_assert(hashmap_contains(m, &k));
    hashmap_free(m);
    arena_free(a);
}

Test(hashmap, contains_returns_false_for_missing_key)
{
    Arena *a = arena_create(256);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        hash_fnv1a,
        hash_eq_bytes,
        arena_allocator(a));
    int k = 42;
    cr_assert(!hashmap_contains(m, &k));
    hashmap_free(m);
    arena_free(a);
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
Test(hashmap, collision_probe_set_and_get)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        always_zero_hash,
        hash_eq_bytes,
        arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    cr_assert_eq(hashmap_len(m), 4);
    for (int i = 1; i <= 4; i++)
    {
        int got;
        cr_assert_eq(hashmap_get(m, &i, &got), SEQC_OK);
        cr_assert_eq(got, i * 10);
    }
    arena_free(a);
}

/*
 * Robin Hood displacement: insert 1,2,3 at their home slots (PSL=1 each),
 * then insert 4 (home=0). When 4 reaches slot 1 its PSL=2 > key2's PSL=1,
 * so 4 steals slot 1 and cascades, displacing key2 then key3 rightward.
 * All four keys must remain retrievable after the cascade.
 */
Test(hashmap, robin_hood_displacement)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        robin_hood_hash,
        hash_eq_bytes,
        arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    cr_assert_eq(hashmap_len(m), 4);
    for (int i = 1; i <= 4; i++)
    {
        int got;
        cr_assert_eq(hashmap_get(m, &i, &got), SEQC_OK);
        cr_assert_eq(got, i * 10);
    }
    arena_free(a);
}

/*
 * Delete a key that is NOT at its home slot (must probe to find it).
 * Also exercises the backward-shift loop: after deletion the chain must
 * be compacted so the remaining keys are still reachable.
 * Uses always_zero_hash so every key is displaced from home.
 */
Test(hashmap, collision_delete_probe_and_backward_shift)
{
    Arena *a = arena_create(4096);
    HashMap *m = hashmap_create(
        sizeof(int),
        sizeof(int),
        always_zero_hash,
        hash_eq_bytes,
        arena_allocator(a));
    for (int i = 1; i <= 4; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    /* key 3 sits at slot 2 (home=0); delete requires probing slots 0,1 first
     * and then backward-shifts key 4 into the vacated slot. */
    int k = 3;
    cr_assert_eq(hashmap_delete(m, &k), SEQC_OK);
    cr_assert_neq(hashmap_get(m, &k, NULL), SEQC_OK);
    for (int i = 1; i <= 4; i++)
    {
        if (i == 3)
            continue;
        int got;
        cr_assert_eq(hashmap_get(m, &i, &got), SEQC_OK);
        cr_assert_eq(got, i * 10);
    }
    cr_assert_eq(hashmap_len(m), 3);
    arena_free(a);
}

/* ---- guard / edge-case tests ------------------------------------------- */

/* hash_fnv1a returns 0 for NULL key or zero key_size. */
Test(hashmap, fnv1a_null_and_zero_size)
{
    int x = 1;
    cr_assert_eq(hash_fnv1a(NULL, sizeof(int)), (size_t)0);
    cr_assert_eq(hash_fnv1a(&x, 0), (size_t)0);
}

/* hash_eq_bytes: zero-size keys are always equal; NULL pointers are not. */
Test(hashmap, eq_bytes_edge_cases)
{
    int a = 1, b = 1;
    cr_assert(hash_eq_bytes(&a, &b, 0));
    cr_assert(!hash_eq_bytes(NULL, &b, sizeof(int)));
    cr_assert(!hash_eq_bytes(&a, NULL, sizeof(int)));
}

/* hashmap_create returns a zero-initialised map when args are invalid. */
Test(hashmap, create_invalid_args_returns_zero_map)
{
    Arena *a = arena_create(256);
    /* key_size == 0 */
    HashMap *m = hashmap_create(
        0, sizeof(int), hash_fnv1a, hash_eq_bytes, arena_allocator(a));
    cr_assert_null(m);
    arena_free(a);
}

/* ---- sys_allocator: exercises all allocator.free branches -------------- */

/*
 * Use sys_allocator so that hashmap_free, hashmap_clear, hashmap_delete, and
 * hashmap_resize_and_rehash all exercise their allocator.free branches.
 */
Test(hashmap, sys_alloc_free_releases_memory)
{
    Allocator al = sys_allocator();
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    for (int i = 0; i < 5; i++)
    {
        int v = i * 10;
        hashmap_set(m, &i, &v);
    }
    cr_assert_eq(hashmap_len(m), 5);
    hashmap_free(m);
    /* memory released — verified by sys_allocator not leaking */
}

Test(hashmap, sys_alloc_clear_frees_entries)
{
    Allocator al = sys_allocator();
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    for (int i = 0; i < 4; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    hashmap_clear(m);
    cr_assert_eq(hashmap_len(m), 0);
    /* map is still usable after clear */
    int k = 99, v = 42;
    hashmap_set(m, &k, &v);
    int got;
    cr_assert_eq(hashmap_get(m, &k, &got), SEQC_OK);
    cr_assert_eq(got, 42);
    hashmap_free(m);
}

Test(hashmap, sys_alloc_resize_frees_old_buckets)
{
    /* Trigger resize (capacity doubles past 75% load) so that
     * hashmap_resize_and_rehash frees the old bucket array and individual
     * keys/values through the sys allocator. */
    Allocator al = sys_allocator();
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    /* Initial cap is 16; resize triggers when len > 12. */
    for (int i = 0; i < 14; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    cr_assert_eq(hashmap_len(m), 14);
    for (int i = 0; i < 14; i++)
    {
        int got;
        cr_assert_eq(hashmap_get(m, &i, &got), SEQC_OK);
        cr_assert_eq(got, i);
    }
    hashmap_free(m);
}

Test(hashmap, is_healthy_normal_load)
{
    Arena *a = arena_create(8192);
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes,
        arena_allocator(a));
    for (int i = 0; i < 50; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    cr_assert(hashmap_is_healthy(m));
    arena_free(a);
}

Test(hashmap, audit_normal_load)
{
    Arena *a = arena_create(8192);
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes,
        arena_allocator(a));
    for (int i = 0; i < 50; i++)
    {
        int v = i;
        hashmap_set(m, &i, &v);
    }
    HashMapStats s = hashmap_audit(m);
    cr_assert_eq(s.len, 50);
    cr_assert(s.cap >= 50);
    cr_assert(s.load_factor > 0.0 && s.load_factor <= 1.0);
    cr_assert(s.max_psl >= 1);
    cr_assert(s.mean_psl >= 1.0);
    cr_assert(s.is_healthy);
    arena_free(a);
}

/* ---- OOM paths --------------------------------------------------------- */

Test(hashmap, create_returns_null_on_oom)
{
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, null_allocator());
    cr_assert_null(m);
}

Test(hashmap, create_returns_null_when_bucket_alloc_fails)
{
    /* alloc #1 (HashMap struct) succeeds, alloc #2 (bucket array) fails */
    OomCtx ctx;
    Allocator al = oom_after_allocator(1, &ctx);
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    cr_assert_null(m);
    /* no leak: struct was freed by hashmap_create on bucket failure */
}

Test(hashmap, set_returns_oom_when_key_alloc_fails)
{
    /* allocs 1-2: create (struct + buckets); alloc 3 (key copy) fails */
    OomCtx ctx;
    Allocator al = oom_after_allocator(2, &ctx);
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    cr_assert_not_null(m);
    int k = 1, v = 1;
    cr_assert_eq(hashmap_set(m, &k, &v), SEQC_OOM);
    cr_assert_eq(hashmap_len(m), 0);
    hashmap_free(m);
}

Test(hashmap, set_returns_oom_when_val_alloc_fails)
{
    /* allocs 1-2: create; alloc 3 (key copy) ok; alloc 4 (val copy) fails */
    OomCtx ctx;
    Allocator al = oom_after_allocator(3, &ctx);
    HashMap *m = hashmap_create(
        sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, al);
    cr_assert_not_null(m);
    int k = 1, v = 1;
    cr_assert_eq(hashmap_set(m, &k, &v), SEQC_OOM);
    cr_assert_eq(hashmap_len(m), 0);
    hashmap_free(m);
}
