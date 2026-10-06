#include "ctt.h"

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/omap.h"
#include "seqc/string.h"

#include "../oom_alloc.h"

/* ---- comparators ------------------------------------------------------- */

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int string_cmp(const void *a, const void *b)
{
    return string_compare(*(const string_t *)a, *(const string_t *)b);
}

/* ---- basic tests ------------------------------------------------------- */

TEST(omap_empty_on_create)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    ASSERT_EQ(0, omap_len(m));
    int mk;
    ASSERT_NE(SEQC_OK, omap_min_key(m, &mk));
    ASSERT_NE(SEQC_OK, omap_max_key(m, &mk));
    growing_arena_destroy(a);
}

TEST(omap_set_and_get)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k1 = 1, v1 = 100;
    int k2 = 2, v2 = 200;
    int k3 = 3, v3 = 300;
    omap_set(m, &k1, &v1);
    omap_set(m, &k2, &v2);
    omap_set(m, &k3, &v3);
    ASSERT_EQ(3, omap_len(m));
    int gv;
    ASSERT_EQ(SEQC_OK, omap_get(m, &k1, &gv)); ASSERT_EQ(100, gv);
    ASSERT_EQ(SEQC_OK, omap_get(m, &k2, &gv)); ASSERT_EQ(200, gv);
    ASSERT_EQ(SEQC_OK, omap_get(m, &k3, &gv)); ASSERT_EQ(300, gv);
    growing_arena_destroy(a);
}

TEST(omap_update_existing_key)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k = 5, v1 = 10, v2 = 99;
    ASSERT_EQ(SEQC_OK, omap_set(m, &k, &v1)); /* inserted */
    ASSERT_EQ(SEQC_OK, omap_set(m, &k, &v2)); /* updated  */
    ASSERT_EQ(1, omap_len(m));
    int gv;
    ASSERT_EQ(SEQC_OK, omap_get(m, &k, &gv));
    ASSERT_EQ(99, gv);
    growing_arena_destroy(a);
}

TEST(omap_get_missing_returns_null)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k = 42;
    ASSERT_NE(SEQC_OK, omap_get(m, &k, NULL));
    growing_arena_destroy(a);
}

TEST(omap_contains_present_and_absent)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int present = 7, absent = 8;
    int v = 0;
    omap_set(m, &present, &v);
    ASSERT_TRUE(omap_contains(m, &present));
    ASSERT_FALSE(omap_contains(m, &absent));
    growing_arena_destroy(a);
}

TEST(omap_min_max_keys)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {5, 1, 8, 3, 9, 2};
    int v = 0;
    for (int i = 0; i < 6; i++)
        omap_set(m, &keys[i], &v);
    int minv, maxv;
    ASSERT_EQ(SEQC_OK, omap_min_key(m, &minv)); ASSERT_EQ(1, minv);
    ASSERT_EQ(SEQC_OK, omap_max_key(m, &maxv)); ASSERT_EQ(9, maxv);
    growing_arena_destroy(a);
}

/* ---- remove ------------------------------------------------------------ */

TEST(omap_remove_existing)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k = 5, v = 50;
    omap_set(m, &k, &v);
    ASSERT_EQ(SEQC_OK, omap_remove(m, &k));
    ASSERT_FALSE(omap_contains(m, &k));
    ASSERT_EQ(0, omap_len(m));
    growing_arena_destroy(a);
}

TEST(omap_remove_nonexistent_returns_0)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int k = 99;
    ASSERT_NE(SEQC_OK, omap_remove(m, &k));
    growing_arena_destroy(a);
}

TEST(omap_remove_rebalances)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 0;
    for (int i = 1; i <= 7; i++)
        omap_set(m, &i, &v);
    for (int i = 1; i <= 6; i++)
        ASSERT_EQ(SEQC_OK, omap_remove(m, &i));
    ASSERT_EQ(1, omap_len(m));
    growing_arena_destroy(a);
}

/* ---- iter -------------------------------------------------------------- */

TEST(omap_iter_ascending_order)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {5, 3, 7, 1, 4, 6, 8};
    for (int i = 0; i < 7; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = omap_iter(m);
    omap_entry_t entries[7];
    size_t n = 0;
    while (it.next(&it, &entries[n]))
        n++;
    iter_drop(&it);
    ASSERT_EQ(7, n);
    for (size_t i = 1; i < n; i++)
        ASSERT_LT(*(int *)entries[i - 1].key, *(int *)entries[i].key);
    /* verify values match keys */
    for (size_t i = 0; i < n; i++)
        ASSERT_EQ(*(int *)entries[i].key * 10, *(int *)entries[i].value);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(omap_iter_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    ASSERT_EQ(0, iter_count(omap_iter(m)));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* ---- string keys ------------------------------------------------------- */

TEST(omap_string_keys)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 2048);
    omap_t *m = omap_create(
        sizeof(string_t), sizeof(int), string_cmp, growing_arena_allocator(a));
    string_t k1 = STRING_LIT("banana");
    string_t k2 = STRING_LIT("apple");
    string_t k3 = STRING_LIT("cherry");
    int v1 = 1, v2 = 2, v3 = 3;
    omap_set(m, &k1, &v1);
    omap_set(m, &k2, &v2);
    omap_set(m, &k3, &v3);
    int gv;
    ASSERT_EQ(SEQC_OK, omap_get(m, &k2, &gv));
    ASSERT_EQ(2, gv);
    /* iterator must yield keys in lexicographic order: apple, banana, cherry */
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = omap_iter(m);
    omap_entry_t e;
    it.next(&it, &e);
    ASSERT_TRUE(string_equals(*(string_t *)e.key, STRING_LIT("apple")));
    it.next(&it, &e);
    ASSERT_TRUE(string_equals(*(string_t *)e.key, STRING_LIT("banana")));
    it.next(&it, &e);
    ASSERT_TRUE(string_equals(*(string_t *)e.key, STRING_LIT("cherry")));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* ---- balance invariant ------------------------------------------------- */

TEST(omap_height_stays_logarithmic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 65536);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int v = 0;
    for (int i = 0; i < 1000; i++)
        omap_set(m, &i, &v);
    ASSERT_EQ(1000, omap_len(m));
    /* AVL height bound: <= 1.44 * log2(n+2) */
    int h = omap_height(m);
    ASSERT_LE(h, 30); /* log2(1000) ~ 10; generous bound */
    growing_arena_destroy(a);
}

TEST(omap_iter_rev_descending)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {4, 2, 6, 1, 3, 5, 7};
    for (int i = 0; i < 7; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    iter_t it = omap_iter_rev(m);
    omap_entry_t e;
    int prev_key = 8; /* larger than any key */
    int n = 0;
    while (it.next(&it, &e))
    {
        ASSERT_LT(*(int *)e.key, prev_key);
        ASSERT_EQ(*(int *)e.key * 10, *(int *)e.value);
        prev_key = *(int *)e.key;
        n++;
    }
    iter_drop(&it);
    ASSERT_EQ(7, n);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(omap_iter_range_mid)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 1; i <= 10; i++)
    {
        int v = i * 100;
        omap_set(m, &i, &v);
    }
    int lo = 3, hi = 7;
    iter_t it = omap_iter_range(m, &lo, &hi);
    omap_entry_t e;
    int n = 0;
    int prev = 0;
    while (it.next(&it, &e))
    {
        int k = *(int *)e.key;
        ASSERT_GE(k, 3);
        ASSERT_LE(k, 7);
        ASSERT_LT(prev, k);
        ASSERT_EQ(k * 100, *(int *)e.value);
        prev = k;
        n++;
    }
    iter_drop(&it);
    ASSERT_EQ(5, n);
    growing_arena_destroy(a);
}

TEST(omap_iter_range_no_lo)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    for (int i = 1; i <= 5; i++)
    {
        int v = 0;
        omap_set(m, &i, &v);
    }
    int hi = 3;
    iter_t it = omap_iter_range(m, NULL, &hi);
    omap_entry_t e;
    int n = 0;
    while (it.next(&it, &e))
        n++;
    iter_drop(&it);
    ASSERT_EQ(3, n); /* 1,2,3 */
    growing_arena_destroy(a);
}

TEST(omap_iter_range_empty_result)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {1, 5, 10};
    for (int i = 0; i < 3; i++)
    {
        int v = 0;
        omap_set(m, &keys[i], &v);
    }
    int lo = 6, hi = 9;
    iter_t it = omap_iter_range(m, &lo, &hi);
    omap_entry_t e;
    ASSERT_TRUE(!it.next(&it, &e));
    iter_drop(&it);
    growing_arena_destroy(a);
}

/* ---- omap_clear -------------------------------------------------------- */

TEST(omap_clear_empties_map)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    omap_clear(m);
    ASSERT_EQ(0, omap_len(m));
    int mk;
    ASSERT_NE(SEQC_OK, omap_min_key(m, &mk));
    ASSERT_NE(SEQC_OK, omap_max_key(m, &mk));
    growing_arena_destroy(a);
}

TEST(omap_clear_allows_reuse)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {3, 1, 5};
    for (int i = 0; i < 3; i++)
    {
        int v = 0;
        omap_set(m, &keys[i], &v);
    }
    omap_clear(m);
    int k = 42, v = 99;
    ASSERT_EQ(SEQC_OK, omap_set(m, &k, &v));
    ASSERT_EQ(1, omap_len(m));
    ASSERT_TRUE(omap_contains(m, &k));
    growing_arena_destroy(a);
}

/* ---- omap_min_entry / omap_max_entry ------------------------------------ */

TEST(omap_min_entry_returns_smallest_key_and_value)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    int min_k, min_v;
    ASSERT_EQ(SEQC_OK, omap_min_entry(m, &min_k, &min_v));
    ASSERT_EQ(1, min_k);
    ASSERT_EQ(10, min_v);
    growing_arena_destroy(a);
}

TEST(omap_max_entry_returns_largest_key_and_value)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, growing_arena_allocator(a));
    int keys[] = {5, 3, 7, 1, 4};
    for (int i = 0; i < 5; i++)
    {
        int v = keys[i] * 10;
        omap_set(m, &keys[i], &v);
    }
    int max_k, max_v;
    ASSERT_EQ(SEQC_OK, omap_max_entry(m, &max_k, &max_v));
    ASSERT_EQ(7, max_k);
    ASSERT_EQ(70, max_v);
    growing_arena_destroy(a);
}

/* ---- OOM paths: an exhausted allocator must not crash ------------------ */

TEST(omap_iter_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, al);
    ASSERT_NOT_NULL(m);
    for (int i = 0; i < 3; i++)
        omap_set(m, &i, &i);
    ctx.remaining = 0; /* exhaust: the iterator's state alloc must fail */
    iter_t it = omap_iter(m);
    ASSERT_NULL(it.next); /* empty iterator, not a NULL deref */
    iter_drop(&it);
    omap_free(m);
}

TEST(omap_iter_rev_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, al);
    ASSERT_NOT_NULL(m);
    for (int i = 0; i < 3; i++)
        omap_set(m, &i, &i);
    ctx.remaining = 0;
    iter_t it = omap_iter_rev(m);
    ASSERT_NULL(it.next);
    iter_drop(&it);
    omap_free(m);
}

TEST(omap_iter_range_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(64, &ctx);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, al);
    ASSERT_NOT_NULL(m);
    for (int i = 0; i < 5; i++)
        omap_set(m, &i, &i);
    int lo = 1, hi = 3;
    ctx.remaining = 0;
    iter_t it = omap_iter_range(m, &lo, &hi);
    ASSERT_NULL(it.next);
    iter_drop(&it);
    omap_free(m);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "omap_test");
}
