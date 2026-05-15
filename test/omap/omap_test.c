#include <gtest/gtest.h>

extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/omap.h"
#include "seqc/string.h"
}

/* ---- comparators ------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int string_cmp(const void *a, const void *b)
{
    return string_compare(*(const String *)a, *(const String *)b);
}

/* ---- basic tests ------------------------------------------------------- */

TEST(omap, empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    EXPECT_EQ(omap_len(m), 0);
    int mk;
    EXPECT_NE(omap_min_key(m, &mk), SEQC_OK);
    EXPECT_NE(omap_max_key(m, &mk), SEQC_OK);
    growing_arena_destroy(a);
}

TEST(omap, set_and_get)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k1 = 1, v1 = 100;
    int k2 = 2, v2 = 200;
    int k3 = 3, v3 = 300;
    omap_set(m, &k1, &v1);
    omap_set(m, &k2, &v2);
    omap_set(m, &k3, &v3);
    EXPECT_EQ(omap_len(m), 3);
    int gv;
    EXPECT_EQ(omap_get(m, &k1, &gv), SEQC_OK); EXPECT_EQ(gv, 100);
    EXPECT_EQ(omap_get(m, &k2, &gv), SEQC_OK); EXPECT_EQ(gv, 200);
    EXPECT_EQ(omap_get(m, &k3, &gv), SEQC_OK); EXPECT_EQ(gv, 300);
    growing_arena_destroy(a);
}

TEST(omap, update_existing_key)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k = 5, v1 = 10, v2 = 99;
    EXPECT_EQ(omap_set(m, &k, &v1), SEQC_OK); /* inserted */
    EXPECT_EQ(omap_set(m, &k, &v2), SEQC_OK); /* updated  */
    EXPECT_EQ(omap_len(m), 1);
    int gv;
    EXPECT_EQ(omap_get(m, &k, &gv), SEQC_OK);
    EXPECT_EQ(gv, 99);
    growing_arena_destroy(a);
}

TEST(omap, get_missing_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k = 42;
    EXPECT_NE(omap_get(m, &k, NULL), SEQC_OK);
    growing_arena_destroy(a);
}

TEST(omap, contains)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int present = 7, absent = 8;
    int v = 0;
    omap_set(m, &present, &v);
    EXPECT_TRUE(omap_contains(m, &present));
    EXPECT_FALSE(omap_contains(m, &absent));
    growing_arena_destroy(a);
}

TEST(omap, min_max_keys)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {5, 1, 8, 3, 9, 2};
    int v = 0;
    for (int i = 0; i < 6; i++)
        omap_set(m, &keys[i], &v);
    int minv, maxv;
    EXPECT_EQ(omap_min_key(m, &minv), SEQC_OK); EXPECT_EQ(minv, 1);
    EXPECT_EQ(omap_max_key(m, &maxv), SEQC_OK); EXPECT_EQ(maxv, 9);
    growing_arena_destroy(a);
}

/* ---- remove ------------------------------------------------------------ */

TEST(omap, remove_existing)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k = 5, v = 50;
    omap_set(m, &k, &v);
    EXPECT_EQ(omap_remove(m, &k), SEQC_OK);
    EXPECT_FALSE(omap_contains(m, &k));
    EXPECT_EQ(omap_len(m), 0);
    growing_arena_destroy(a);
}

TEST(omap, remove_nonexistent_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k = 99;
    EXPECT_NE(omap_remove(m, &k), SEQC_OK);
    growing_arena_destroy(a);
}

TEST(omap, remove_rebalances)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 0;
    for (int i = 1; i <= 7; i++)
        omap_set(m, &i, &v);
    for (int i = 1; i <= 6; i++)
        EXPECT_EQ(omap_remove(m, &i), SEQC_OK);
    EXPECT_EQ(omap_len(m), 1);
    growing_arena_destroy(a);
}

/* ---- iter -------------------------------------------------------------- */

TEST(omap, iter_ascending_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = omap_iter(m);
    OMapEntry entries[7];
    size_t n = 0;
    while (it.next(&it, &entries[n]))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 7);
    for (size_t i = 1; i < n; i++)
        EXPECT_LT(*(int *)entries[i - 1].key, *(int *)entries[i].key);
    /* verify values match keys */
    for (size_t i = 0; i < n; i++)
        EXPECT_EQ(*(int *)entries[i].value, *(int *)entries[i].key * 10);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(omap, iter_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    EXPECT_EQ(iter_count(omap_iter(m)), 0);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* ---- string keys ------------------------------------------------------- */

TEST(omap, string_keys)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 2048);
    OMap *m = omap_create(
        sizeof(String), sizeof(int), string_cmp, growing_arena_allocator(a));
    String k1 = STRING_LIT("banana");
    String k2 = STRING_LIT("apple");
    String k3 = STRING_LIT("cherry");
    int v1 = 1, v2 = 2, v3 = 3;
    omap_set(m, &k1, &v1);
    omap_set(m, &k2, &v2);
    omap_set(m, &k3, &v3);
    int gv;
    EXPECT_EQ(omap_get(m, &k2, &gv), SEQC_OK);
    EXPECT_EQ(gv, 2);
    /* iterator must yield keys in lexicographic order: apple, banana, cherry */
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = omap_iter(m);
    OMapEntry e;
    it.next(&it, &e);
    EXPECT_TRUE(string_equals(*(String *)e.key, STRING_LIT("apple")));
    it.next(&it, &e);
    EXPECT_TRUE(string_equals(*(String *)e.key, STRING_LIT("banana")));
    it.next(&it, &e);
    EXPECT_TRUE(string_equals(*(String *)e.key, STRING_LIT("cherry")));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* ---- balance invariant ------------------------------------------------- */

TEST(omap, height_stays_logarithmic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 65536);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 0;
    for (int i = 0; i < 1000; i++)
        omap_set(m, &i, &v);
    EXPECT_EQ(omap_len(m), 1000);
    /* AVL height bound: <= 1.44 * log2(n+2) */
    int h = omap_height(m);
    EXPECT_LE(h, 30); /* log2(1000) ~ 10; generous bound */
    growing_arena_destroy(a);
}

TEST(omap, iter_rev_descending)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {4, 2, 6, 1, 3, 5, 7};
    for (int i = 0; i < 7; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = omap_iter_rev(m);
    OMapEntry e;
    int prev_key = 8; /* larger than any key */
    int n = 0;
    while (it.next(&it, &e))
    {
        EXPECT_LT(*(int *)e.key, prev_key);
        EXPECT_EQ(*(int *)e.value, *(int *)e.key * 10);
        prev_key = *(int *)e.key;
        n++;
    }
    iter_drop(&it);
    EXPECT_EQ(n, 7);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(omap, iter_range_mid)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 1; i <= 10; i++)
    {
        int v = i * 100;
        omap_set(m, &i, &v);
    }
    int lo = 3, hi = 7;
    Iter it = omap_iter_range(m, &lo, &hi);
    OMapEntry e;
    int n = 0;
    int prev = 0;
    while (it.next(&it, &e))
    {
        int k = *(int *)e.key;
        EXPECT_GE(k, 3);
        EXPECT_LE(k, 7);
        EXPECT_LT(prev, k);
        EXPECT_EQ(*(int *)e.value, k * 100);
        prev = k;
        n++;
    }
    iter_drop(&it);
    EXPECT_EQ(n, 5);
    growing_arena_destroy(a);
}

TEST(omap, iter_range_no_lo)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 1; i <= 5; i++)
    {
        int v = 0;
        omap_set(m, &i, &v);
    }
    int hi = 3;
    Iter it = omap_iter_range(m, NULL, &hi);
    OMapEntry e;
    int n = 0;
    while (it.next(&it, &e))
        n++;
    iter_drop(&it);
    EXPECT_EQ(n, 3); /* 1,2,3 */
    growing_arena_destroy(a);
}

TEST(omap, iter_range_empty_result)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {1, 5, 10};
    for (int i = 0; i < 3; i++)
    {
        int v = 0;
        omap_set(m, &keys[i], &v);
    }
    int lo = 6, hi = 9;
    Iter it = omap_iter_range(m, &lo, &hi);
    OMapEntry e;
    EXPECT_TRUE(!it.next(&it, &e));
    iter_drop(&it);
    growing_arena_destroy(a);
}

/* ---- omap_clear -------------------------------------------------------- */

TEST(omap, clear_empties_map)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    omap_clear(m);
    EXPECT_EQ(omap_len(m), 0);
    int mk;
    EXPECT_NE(omap_min_key(m, &mk), SEQC_OK);
    EXPECT_NE(omap_max_key(m, &mk), SEQC_OK);
    growing_arena_destroy(a);
}

TEST(omap, clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
    {
        int v = 0;
        omap_set(m, &keys[i], &v);
    }
    omap_clear(m);
    int k = 42, v = 99;
    EXPECT_EQ(omap_set(m, &k, &v), SEQC_OK);
    EXPECT_EQ(omap_len(m), 1);
    EXPECT_TRUE(omap_contains(m, &k));
    growing_arena_destroy(a);
}

/* ---- omap_min_entry / omap_max_entry ------------------------------------ */

TEST(omap, min_entry_returns_smallest_key_and_value)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    int min_k, min_v;
    EXPECT_EQ(omap_min_entry(m, &min_k, &min_v), SEQC_OK);
    EXPECT_EQ(min_k, 1);
    EXPECT_EQ(min_v, 10);
    growing_arena_destroy(a);
}

TEST(omap, max_entry_returns_largest_key_and_value)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    OMap *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    int max_k, max_v;
    EXPECT_EQ(omap_max_entry(m, &max_k, &max_v), SEQC_OK);
    EXPECT_EQ(max_k, 7);
    EXPECT_EQ(max_v, 70);
    growing_arena_destroy(a);
}
