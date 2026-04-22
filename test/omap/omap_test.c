#include <criterion/criterion.h>

#include "arena/arena.h"
#include "omap/omap.h"
#include "string/string.h"

/* ---- comparators ------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static int string_cmp(const void *a, const void *b)
{
    return string_compare(*(const String *)a, *(const String *)b);
}

/* ---- basic tests ------------------------------------------------------- */

Test(omap, empty_on_create)
{
    Arena *a = arena_create(256);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    cr_assert_eq(omap_len(&m), 0);
    cr_assert_null(omap_min_key(&m));
    cr_assert_null(omap_max_key(&m));
    arena_free(a);
}

Test(omap, set_and_get)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int k1 = 1, v1 = 100;
    int k2 = 2, v2 = 200;
    int k3 = 3, v3 = 300;
    omap_set(&m, &k1, &v1);
    omap_set(&m, &k2, &v2);
    omap_set(&m, &k3, &v3);
    cr_assert_eq(omap_len(&m), 3);
    cr_assert_eq(*(int *)omap_get(&m, &k1), 100);
    cr_assert_eq(*(int *)omap_get(&m, &k2), 200);
    cr_assert_eq(*(int *)omap_get(&m, &k3), 300);
    arena_free(a);
}

Test(omap, update_existing_key)
{
    Arena *a = arena_create(512);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int k = 5, v1 = 10, v2 = 99;
    cr_assert_eq(omap_set(&m, &k, &v1), 1); /* inserted */
    cr_assert_eq(omap_set(&m, &k, &v2), 0); /* updated  */
    cr_assert_eq(omap_len(&m), 1);
    cr_assert_eq(*(int *)omap_get(&m, &k), 99);
    arena_free(a);
}

Test(omap, get_missing_returns_null)
{
    Arena *a = arena_create(256);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int k = 42;
    cr_assert_null(omap_get(&m, &k));
    arena_free(a);
}

Test(omap, contains)
{
    Arena *a = arena_create(512);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int present = 7, absent = 8;
    int v = 0;
    omap_set(&m, &present, &v);
    cr_assert(omap_contains(&m, &present));
    cr_assert_not(omap_contains(&m, &absent));
    arena_free(a);
}

Test(omap, min_max_keys)
{
    Arena *a = arena_create(512);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int keys[] = {5, 1, 8, 3, 9, 2};
    int v = 0;
    for (int i = 0; i < 6; i++)
        omap_set(&m, &keys[i], &v);
    cr_assert_eq(*(int *)omap_min_key(&m), 1);
    cr_assert_eq(*(int *)omap_max_key(&m), 9);
    arena_free(a);
}

/* ---- remove ------------------------------------------------------------ */

Test(omap, remove_existing)
{
    Arena *a = arena_create(512);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int k = 5, v = 50;
    omap_set(&m, &k, &v);
    cr_assert(omap_remove(&m, &k));
    cr_assert_not(omap_contains(&m, &k));
    cr_assert_eq(omap_len(&m), 0);
    arena_free(a);
}

Test(omap, remove_nonexistent_returns_0)
{
    Arena *a = arena_create(256);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int k = 99;
    cr_assert_not(omap_remove(&m, &k));
    arena_free(a);
}

Test(omap, remove_rebalances)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int v = 0;
    for (int i = 1; i <= 7; i++)
        omap_set(&m, &i, &v);
    for (int i = 1; i <= 6; i++)
        cr_assert(omap_remove(&m, &i));
    cr_assert_eq(omap_len(&m), 1);
    arena_free(a);
}

/* ---- iter -------------------------------------------------------------- */

Test(omap, iter_ascending_order)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int keys[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
    {
        int v = keys[i] * 10;
        omap_set(&m, &keys[i], &v);
    }
    Scratch sc = arena_scratch_push(a);
    Iter it = omap_iter(&m);
    OMapEntry entries[7];
    size_t n = 0;
    while (it.next(&it, &entries[n]))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 7);
    for (size_t i = 1; i < n; i++)
        cr_assert_lt(*(int *)entries[i - 1].key, *(int *)entries[i].key);
    /* verify values match keys */
    for (size_t i = 0; i < n; i++)
        cr_assert_eq(*(int *)entries[i].value, *(int *)entries[i].key * 10);
    arena_scratch_pop(&sc);
    arena_free(a);
}

Test(omap, iter_empty)
{
    Arena *a = arena_create(256);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    Scratch sc = arena_scratch_push(a);
    cr_assert_eq(iter_count(omap_iter(&m)), 0);
    arena_scratch_pop(&sc);
    arena_free(a);
}

/* ---- string keys ------------------------------------------------------- */

Test(omap, string_keys)
{
    Arena *a = arena_create(2048);
    OMap m = omap_create(
        sizeof(String), sizeof(int), string_cmp, arena_allocator(a));
    String k1 = STRING_LIT("banana");
    String k2 = STRING_LIT("apple");
    String k3 = STRING_LIT("cherry");
    int v1 = 1, v2 = 2, v3 = 3;
    omap_set(&m, &k1, &v1);
    omap_set(&m, &k2, &v2);
    omap_set(&m, &k3, &v3);
    cr_assert_eq(*(int *)omap_get(&m, &k2), 2);
    /* iterator must yield keys in lexicographic order: apple, banana, cherry */
    Scratch sc = arena_scratch_push(a);
    Iter it = omap_iter(&m);
    OMapEntry e;
    it.next(&it, &e);
    cr_assert(string_equals(*(String *)e.key, STRING_LIT("apple")));
    it.next(&it, &e);
    cr_assert(string_equals(*(String *)e.key, STRING_LIT("banana")));
    it.next(&it, &e);
    cr_assert(string_equals(*(String *)e.key, STRING_LIT("cherry")));
    iter_drop(&it);
    arena_scratch_pop(&sc);
    arena_free(a);
}

/* ---- balance invariant ------------------------------------------------- */

Test(omap, height_stays_logarithmic)
{
    Arena *a = arena_create(65536);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int v = 0;
    for (int i = 0; i < 1000; i++)
        omap_set(&m, &i, &v);
    cr_assert_eq(omap_len(&m), 1000);
    /* AVL height bound: <= 1.44 * log2(n+2) */
    int h = omap_height(&m);
    cr_assert_leq(h, 30); /* log2(1000) ~ 10; generous bound */
    arena_free(a);
}

Test(omap, iter_rev_descending)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int keys[] = {4, 2, 6, 1, 3, 5, 7};
    for (int i = 0; i < 7; i++)
    {
        int v = keys[i] * 10;
        omap_set(&m, &keys[i], &v);
    }
    Scratch sc = arena_scratch_push(a);
    Iter it = omap_iter_rev(&m);
    OMapEntry e;
    int prev_key = 8; /* larger than any key */
    int n = 0;
    while (it.next(&it, &e))
    {
        cr_assert_lt(*(int *)e.key, prev_key);
        cr_assert_eq(*(int *)e.value, *(int *)e.key * 10);
        prev_key = *(int *)e.key;
        n++;
    }
    iter_drop(&it);
    cr_assert_eq(n, 7);
    arena_scratch_pop(&sc);
    arena_free(a);
}

Test(omap, iter_range_mid)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    for (int i = 1; i <= 10; i++)
    {
        int v = i * 100;
        omap_set(&m, &i, &v);
    }
    int lo = 3, hi = 7;
    Iter it = omap_iter_range(&m, &lo, &hi);
    OMapEntry e;
    int n = 0;
    int prev = 0;
    while (it.next(&it, &e))
    {
        int k = *(int *)e.key;
        cr_assert_geq(k, 3);
        cr_assert_leq(k, 7);
        cr_assert_lt(prev, k);
        cr_assert_eq(*(int *)e.value, k * 100);
        prev = k;
        n++;
    }
    iter_drop(&it);
    cr_assert_eq(n, 5);
    arena_free(a);
}

Test(omap, iter_range_no_lo)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    for (int i = 1; i <= 5; i++)
    {
        int v = 0;
        omap_set(&m, &i, &v);
    }
    int hi = 3;
    Iter it = omap_iter_range(&m, NULL, &hi);
    OMapEntry e;
    int n = 0;
    while (it.next(&it, &e))
        n++;
    iter_drop(&it);
    cr_assert_eq(n, 3); /* 1,2,3 */
    arena_free(a);
}

Test(omap, iter_range_empty_result)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int keys[] = {1, 5, 10};
    for (int i = 0; i < 3; i++)
    {
        int v = 0;
        omap_set(&m, &keys[i], &v);
    }
    int lo = 6, hi = 9;
    Iter it = omap_iter_range(&m, &lo, &hi);
    OMapEntry e;
    cr_assert(!it.next(&it, &e));
    iter_drop(&it);
    arena_free(a);
}

/* ---- omap_clear -------------------------------------------------------- */

Test(omap, clear_empties_map)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int keys[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
    {
        int v = keys[i] * 10;
        omap_set(&m, &keys[i], &v);
    }
    omap_clear(&m);
    cr_assert_eq(omap_len(&m), 0);
    cr_assert_null(omap_min_key(&m));
    cr_assert_null(omap_max_key(&m));
    arena_free(a);
}

Test(omap, clear_allows_reuse)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int keys[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
    {
        int v = 0;
        omap_set(&m, &keys[i], &v);
    }
    omap_clear(&m);
    int k = 42, v = 99;
    cr_assert(omap_set(&m, &k, &v));
    cr_assert_eq(omap_len(&m), 1);
    cr_assert(omap_contains(&m, &k));
    arena_free(a);
}

/* ---- omap_min_entry / omap_max_entry ------------------------------------ */

Test(omap, min_entry_returns_smallest_key_and_value)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int keys[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
    {
        int v = keys[i] * 10;
        omap_set(&m, &keys[i], &v);
    }
    OMapEntry e = omap_min_entry(&m);
    cr_assert_eq(*(int *)e.key, 1);
    cr_assert_eq(*(int *)e.value, 10);
    arena_free(a);
}

Test(omap, max_entry_returns_largest_key_and_value)
{
    Arena *a = arena_create(1024);
    OMap m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));
    int keys[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
    {
        int v = keys[i] * 10;
        omap_set(&m, &keys[i], &v);
    }
    OMapEntry e = omap_max_entry(&m);
    cr_assert_eq(*(int *)e.key, 7);
    cr_assert_eq(*(int *)e.value, 70);
    arena_free(a);
}
