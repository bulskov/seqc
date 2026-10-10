#include "ctt.h"

#include <stdio.h>

#include "arena/debug_allocator.h"
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/hashmap.h"
#include "seqc/string.h"

#include "../oom_alloc.h"

/* Predicate: keep only non-empty string tokens (for skip-empty splitting). */
static bool non_empty_str(const void *elem, void *ctx)
{
    (void)ctx;
    return ((const string_t *)elem)->len > 0;
}

/* --- Construction ------------------------------------------------------- */

TEST(string_from_cstr_length)
{
    string_t s = string_view_cstr("hello");
    ASSERT_EQ(5, s.len);
}

TEST(string_from_cstr_copies_content)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    char buf[] = "hello";
    string_t s = string_from_cstr(buf, growing_arena_allocator(a));
    ASSERT_EQ(5, s.len);
    buf[0] = 'X';             /* mutate source */
    ASSERT_EQ('h', s.ptr[0]); /* copy unaffected */
    growing_arena_destroy(a);
}

TEST(string_from_lit_macro)
{
    string_t s = STRING_LIT("world");
    ASSERT_EQ(5, s.len);
}

TEST(string_to_cstr_is_null_terminated)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    string_t s = STRING_LIT("hi");
    const char *cs = string_to_cstr(s, growing_arena_allocator(a));
    ASSERT_EQ('\0', cs[2]);
    ASSERT_STR_EQ("hi", cs);
    growing_arena_destroy(a);
}

TEST(string_copy_is_independent)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    char buf[] = "mutable";
    string_t s = string_view_cstr(buf);
    string_t c = string_copy(s, growing_arena_allocator(a));
    buf[0] = 'X';
    ASSERT_EQ('m', c.ptr[0]); /* copy unaffected */
    growing_arena_destroy(a);
}

/* --- Comparison --------------------------------------------------------- */

TEST(string_equals_same_content)
{
    ASSERT_TRUE(string_equals(STRING_LIT("abc"), STRING_LIT("abc")));
}

TEST(string_equals_different_content)
{
    ASSERT_FALSE(string_equals(STRING_LIT("abc"), STRING_LIT("abd")));
}

TEST(string_equals_different_length)
{
    ASSERT_FALSE(string_equals(STRING_LIT("abc"), STRING_LIT("ab")));
}

TEST(string_equals_case_insensitive_ignores_case)
{
    ASSERT_TRUE(string_equals_case_insensitive(
        STRING_LIT("Hello World"), STRING_LIT("hELLO wORLD")));
}

TEST(string_equals_case_insensitive_different_content)
{
    ASSERT_FALSE(
        string_equals_case_insensitive(STRING_LIT("abc"), STRING_LIT("ABD")));
}

TEST(string_equals_case_insensitive_different_length)
{
    ASSERT_FALSE(
        string_equals_case_insensitive(STRING_LIT("abc"), STRING_LIT("AB")));
}

TEST(string_equals_case_insensitive_non_letters_exact)
{
    ASSERT_TRUE(string_equals_case_insensitive(
        STRING_LIT("a-1_b"), STRING_LIT("A-1_B")));
    ASSERT_FALSE(
        string_equals_case_insensitive(STRING_LIT("a-1"), STRING_LIT("a_1")));
}

TEST(string_equals_case_insensitive_empty)
{
    ASSERT_TRUE(string_equals_case_insensitive(STRING_LIT(""), STRING_LIT("")));
}

TEST(string_compare_ordering)
{
    ASSERT_LT(string_compare(STRING_LIT("abc"), STRING_LIT("abd")), 0);
    ASSERT_GT(string_compare(STRING_LIT("b"), STRING_LIT("a")), 0);
    ASSERT_EQ(0, string_compare(STRING_LIT("x"), STRING_LIT("x")));
}

/* --- Query -------------------------------------------------------------- */

TEST(string_starts_with_true)
{
    ASSERT_TRUE(string_starts_with(STRING_LIT("foobar"), STRING_LIT("foo")));
}

TEST(string_starts_with_false)
{
    ASSERT_FALSE(string_starts_with(STRING_LIT("foobar"), STRING_LIT("bar")));
}

TEST(string_ends_with_true)
{
    ASSERT_TRUE(string_ends_with(STRING_LIT("foobar"), STRING_LIT("bar")));
}

TEST(string_ends_with_false)
{
    ASSERT_FALSE(string_ends_with(STRING_LIT("foobar"), STRING_LIT("foo")));
}

TEST(string_contains_true)
{
    ASSERT_TRUE(
        string_contains(STRING_LIT("hello world"), STRING_LIT("world")));
}

TEST(string_contains_false)
{
    ASSERT_FALSE(string_contains(STRING_LIT("hello"), STRING_LIT("xyz")));
}

TEST(string_find_returns_index)
{
    ASSERT_EQ(2, string_find(STRING_LIT("abcdef"), STRING_LIT("cd")));
}

TEST(string_find_not_found)
{
    ASSERT_EQ(
        STRING_NOT_FOUND, string_find(STRING_LIT("abc"), STRING_LIT("z")));
}

/* --- Views --------------------------------------------------------------- */

TEST(string_slice_zero_copy)
{
    string_t s = STRING_LIT("hello world");
    string_t sub = string_slice(s, 6, 11);
    ASSERT_EQ(5, sub.len);
    ASSERT_TRUE(string_equals(sub, STRING_LIT("world")));
    ASSERT_PTR_EQ(s.ptr + 6, sub.ptr); /* same pointer — no copy */
}

TEST(string_trim_removes_whitespace)
{
    ASSERT_TRUE(string_equals(
        string_trim(STRING_LIT("  hello\n  ")), STRING_LIT("hello")));
    string_t s = STRING_LIT("  a \t\n  ");
    s = string_trim(s);
    ASSERT_EQ(1, s.len);
}

TEST(string_trim_left_only)
{
    string_t t = string_trim_left(STRING_LIT("  hi"));
    ASSERT_TRUE(string_equals(t, STRING_LIT("hi")));
}

TEST(string_trim_right_only)
{
    string_t t = string_trim_right(STRING_LIT("hi  "));
    ASSERT_TRUE(string_equals(t, STRING_LIT("hi")));
}

TEST(string_trim_no_whitespace)
{
    ASSERT_TRUE(
        string_equals(string_trim(STRING_LIT("abc")), STRING_LIT("abc")));
}

/* --- Builder ------------------------------------------------------------ */

TEST(string_builder_append_str)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    strbuf_append(sb, STRING_LIT("hello"));
    strbuf_append_char(sb, ' ');
    strbuf_append_cstr(sb, "world");
    string_t result = strbuf_view(sb);
    ASSERT_TRUE(string_equals(result, STRING_LIT("hello world")));
    growing_arena_destroy(a);
}

TEST(string_builder_len_tracks_appends)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    ASSERT_EQ(0u, strbuf_len(sb));
    strbuf_append(sb, STRING_LIT("hello"));
    ASSERT_EQ(5u, strbuf_len(sb));
    strbuf_append_char(sb, ' ');
    strbuf_append_int(sb, 42);
    ASSERT_EQ(8u, strbuf_len(sb));
    ASSERT_EQ(strbuf_view(sb).len, strbuf_len(sb));
    growing_arena_destroy(a);
}

TEST(string_builder_clear_empties_it)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    strbuf_append(sb, STRING_LIT("hello"));
    strbuf_clear(sb);
    ASSERT_EQ(0u, strbuf_len(sb));
    ASSERT_EQ(0u, strbuf_view(sb).len);
    growing_arena_destroy(a);
}

TEST(string_builder_truncate_undoes_appends)
{
    allocator_t heap = sys_allocator();
    strbuf_t *sb = strbuf_create(heap);
    strbuf_append(sb, STRING_LIT("/home"));
    size_t mark = strbuf_len(sb);
    strbuf_append(sb, STRING_LIT("/me/projects"));
    strbuf_truncate(sb, mark);
    ASSERT_TRUE(string_equals(strbuf_view(sb), STRING_LIT("/home")));
    strbuf_truncate(sb, 100); /* never grows */
    ASSERT_EQ(5u, strbuf_len(sb));
    strbuf_append(sb, STRING_LIT("/you"));
    ASSERT_TRUE(string_equals(strbuf_view(sb), STRING_LIT("/home/you")));
    strbuf_truncate(sb, 0);
    ASSERT_TRUE(strbuf_is_empty(sb));
    strbuf_truncate(NULL, 0);
    strbuf_destroy(sb);
}

TEST(string_builder_is_reusable_after_clear)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    strbuf_append(sb, STRING_LIT("first"));
    strbuf_clear(sb);
    strbuf_append(sb, STRING_LIT("second"));
    ASSERT_TRUE(string_equals(strbuf_view(sb), STRING_LIT("second")));
    growing_arena_destroy(a);
}

/* Clear keeps the buffer: building the same length again allocates
 * nothing. */
TEST(string_builder_clear_keeps_capacity)
{
    growing_arena_t arena;
    growing_arena_init(&arena, 4096);
    debug_allocator_t dbg;
    debug_allocator_init(&dbg, growing_arena_allocator(&arena));
    strbuf_t *sb = strbuf_create(debug_allocator_allocator(&dbg));
    strbuf_append(sb, STRING_LIT("a reasonably long string to build"));
    size_t allocs = debug_allocator_stats(&dbg).alloc_count;
    strbuf_clear(sb);
    strbuf_append(sb, STRING_LIT("a reasonably long string to build"));
    ASSERT_EQ(allocs, debug_allocator_stats(&dbg).alloc_count);
    growing_arena_destroy(&arena);
}

/* strbuf_destroy gives back everything strbuf_create and the appends took. */
TEST(string_builder_free_releases_everything)
{
    growing_arena_t arena;
    growing_arena_init(&arena, 4096);
    debug_allocator_t dbg;
    debug_allocator_init(&dbg, growing_arena_allocator(&arena));
    strbuf_t *sb = strbuf_create(debug_allocator_allocator(&dbg));
    ASSERT_NOT_NULL(sb);
    for (int i = 0; i < 100; ++i)
    {
        strbuf_append(sb, STRING_LIT("grow the buffer "));
    }
    ASSERT_GT(debug_allocator_stats(&dbg).bytes_live, 0u);
    strbuf_destroy(sb);
    ASSERT_EQ(0u, debug_allocator_stats(&dbg).bytes_live);
    growing_arena_destroy(&arena);
}

TEST(string_builder_clear_and_free_accept_null)
{
    strbuf_clear(NULL);
    strbuf_destroy(NULL);
}

/* An allocator that fails returns NULL from strbuf_create — and does not
 * hand out a builder whose buffer is missing. */
TEST(string_builder_create_reports_oom)
{
    ASSERT_NULL(strbuf_create(ALLOCATOR_NULL));

    /* The builder itself succeeds, its buffer does not: still NULL, and the
     * builder is given back (LeakSanitizer checks under ./test.sh asan). */
    oom_ctx_t ctx = {0};
    ASSERT_NULL(strbuf_create(oom_after_allocator(1, &ctx)));
}

TEST(string_builder_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    string_t result = strbuf_view(sb);
    ASSERT_EQ(0, result.len);
    growing_arena_destroy(a);
}

/* --- iter_t sources ------------------------------------------------------- */

TEST(string_chars_count)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t n =
        iter_count(string_chars(STRING_LIT("hello"), scratch_allocator(&sc)));
    ASSERT_EQ(5, n);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_chars_rev_yields_reverse_order)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = string_chars_rev(STRING_LIT("abc"), scratch_allocator(&sc));
    char c;
    it.next(&it, &c);
    ASSERT_EQ('c', c);
    it.next(&it, &c);
    ASSERT_EQ('b', c);
    it.next(&it, &c);
    ASSERT_EQ('a', c);
    ASSERT_FALSE(it.next(&it, &c));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_basic)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    string_t parts[3];
    size_t i = 0;
    iter_t it = string_split_substr(
        STRING_LIT("a,b,c"), STRING_LIT(","), scratch_allocator(&sc));
    while (it.next(&it, &parts[i]))
    {
        i++;
    }
    iter_destroy(&it);
    ASSERT_EQ(3, i);
    ASSERT_TRUE(string_equals(parts[0], STRING_LIT("a")));
    ASSERT_TRUE(string_equals(parts[1], STRING_LIT("b")));
    ASSERT_TRUE(string_equals(parts[2], STRING_LIT("c")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_trailing_delim)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(string_split_substr(
        STRING_LIT("a,b,"), STRING_LIT(","), scratch_allocator(&sc)));
    ASSERT_EQ(3, n); /* "a", "b", "" */
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_no_delim)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    string_t token;
    iter_t it = string_split_substr(
        STRING_LIT("hello"), STRING_LIT(","), scratch_allocator(&sc));
    ASSERT_TRUE(it.next(&it, &token));
    ASSERT_TRUE(string_equals(token, STRING_LIT("hello")));
    ASSERT_FALSE(it.next(&it, &token));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_null_string_yields_one_empty_token)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    string_t null_str = {NULL, 0};
    /* The iterator is consumed after string_split_substr returns, so its
     * yielded token must not point into string_split_substr's own stack frame.
     */
    iter_t it =
        string_split_substr(null_str, STRING_LIT(","), scratch_allocator(&sc));
    string_t token = {(char *)1, 999}; /* poison: must be overwritten */
    ASSERT_TRUE(it.next(&it, &token));
    ASSERT_EQ(0u, token.len);
    ASSERT_FALSE(it.next(&it, &token));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_substr_empty_delim_yields_whole)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    string_t token;
    iter_t it = string_split_substr(
        STRING_LIT("abc"), STRING_LIT(""), scratch_allocator(&sc));
    ASSERT_TRUE(it.next(&it, &token));
    ASSERT_TRUE(string_equals(token, STRING_LIT("abc")));
    ASSERT_FALSE(it.next(&it, &token));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_any_charset)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    string_t parts[3];
    size_t i = 0;
    /* split on either ',' or ';' */
    iter_t it = string_split_any(
        STRING_LIT("a,b;c"), STRING_LIT(",;"), scratch_allocator(&sc));
    while (it.next(&it, &parts[i]))
    {
        i++;
    }
    iter_destroy(&it);
    ASSERT_EQ(3u, i);
    ASSERT_TRUE(string_equals(parts[0], STRING_LIT("a")));
    ASSERT_TRUE(string_equals(parts[1], STRING_LIT("b")));
    ASSERT_TRUE(string_equals(parts[2], STRING_LIT("c")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_any_keeps_empties_each_char_is_boundary)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    string_t parts[5];
    size_t i = 0;
    /* "a, b" on set ", ": ',' at 1 and ' ' at 2 are separate boundaries, so
     * the empty run between them yields an empty token. */
    iter_t it = string_split_any(
        STRING_LIT("a, b"), STRING_LIT(", "), scratch_allocator(&sc));
    while (it.next(&it, &parts[i]))
    {
        i++;
    }
    iter_destroy(&it);
    ASSERT_EQ(3u, i);
    ASSERT_TRUE(string_equals(parts[0], STRING_LIT("a")));
    ASSERT_EQ(0u, parts[1].len);
    ASSERT_TRUE(string_equals(parts[2], STRING_LIT("b")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_any_leading_and_trailing_yield_empties)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(string_split_any(
        STRING_LIT(" a "), STRING_LIT(" "), scratch_allocator(&sc)));
    ASSERT_EQ(3u, n); /* "", "a", "" */
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_any_whitespace_tokenize_with_filter)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    string_t parts[8];
    size_t i = 0;
    /* skip-empty tokenisation = keep-empty split composed with iter_filter */
    iter_t it = iter_filter(
        string_split_any(
            STRING_LIT("  the\tquick \nbrown  "),
            STRING_LIT(" \t\n"),
            scratch_allocator(&sc)),
        non_empty_str,
        NULL);
    while (it.next(&it, &parts[i]))
    {
        i++;
    }
    iter_destroy(&it);
    ASSERT_EQ(3u, i);
    ASSERT_TRUE(string_equals(parts[0], STRING_LIT("the")));
    ASSERT_TRUE(string_equals(parts[1], STRING_LIT("quick")));
    ASSERT_TRUE(string_equals(parts[2], STRING_LIT("brown")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_any_empty_set_yields_whole)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    string_t token;
    iter_t it = string_split_any(
        STRING_LIT("abc"), STRING_LIT(""), scratch_allocator(&sc));
    ASSERT_TRUE(it.next(&it, &token));
    ASSERT_TRUE(string_equals(token, STRING_LIT("abc")));
    ASSERT_FALSE(it.next(&it, &token));
    iter_destroy(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_split_any_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(0, &ctx); /* state alloc fails */
    iter_t it = string_split_any(STRING_LIT("a b c"), STRING_LIT(" "), al);
    ASSERT_NULL(it.next); /* empty iterator, not a NULL deref */
    iter_destroy(&it);
}

/* --- hashmap_t with string_t keys -------------------------------------------
 */

TEST(string_hashmap_string_keys)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 1024);
    hashmap_t *map = hashmap_create(
        sizeof(string_t),
        sizeof(int),
        string_hash,
        string_key_eq,
        growing_arena_allocator(a));

    string_t k1 = STRING_LIT("foo");
    string_t k2 = STRING_LIT("bar");
    int v1 = 1, v2 = 2;
    hashmap_set(map, &k1, &v1);
    hashmap_set(map, &k2, &v2);

    int g1, g2;
    ASSERT_EQ(SEQC_OK, hashmap_get(map, &k1, &g1));
    ASSERT_EQ(1, g1);
    ASSERT_EQ(SEQC_OK, hashmap_get(map, &k2, &g2));
    ASSERT_EQ(2, g2);

    /* Key from different pointer but same content must still hit */
    char buf[] = "foo";
    string_t k1_copy = string_view_cstr(buf);
    int g1c;
    ASSERT_EQ(SEQC_OK, hashmap_get(map, &k1_copy, &g1c));
    ASSERT_EQ(1, g1c);

    hashmap_destroy(map);
    growing_arena_destroy(a);
}

/* --- strbuf_append_int / strbuf_append_fmt
 * ------------------------------------- */

TEST(string_strbuf_append_int_positive)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    strbuf_append_int(sb, 42);
    string_t result = strbuf_view(sb);
    ASSERT_TRUE(string_equals(result, STRING_LIT("42")));
    growing_arena_destroy(a);
}

TEST(string_strbuf_append_int_negative)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    strbuf_append_int(sb, -123);
    string_t result = strbuf_view(sb);
    ASSERT_TRUE(string_equals(result, STRING_LIT("-123")));
    growing_arena_destroy(a);
}

TEST(string_strbuf_append_int_zero)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    strbuf_append_int(sb, 0);
    string_t result = strbuf_view(sb);
    ASSERT_TRUE(string_equals(result, STRING_LIT("0")));
    growing_arena_destroy(a);
}

TEST(string_strbuf_append_fmt_basic)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    strbuf_append_fmt(sb, "hello %s, you are %d years old", "world", 30);
    string_t result = strbuf_view(sb);
    ASSERT_TRUE(
        string_equals(result, STRING_LIT("hello world, you are 30 years old")));
    growing_arena_destroy(a);
}

TEST(string_strbuf_append_fmt_compose)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    strbuf_t *sb = strbuf_create(growing_arena_allocator(a));
    strbuf_append_cstr(sb, "x=");
    strbuf_append_fmt(sb, "%d", 7);
    strbuf_append_cstr(sb, ", y=");
    strbuf_append_fmt(sb, "%.2f", 3.14);
    string_t result = strbuf_view(sb);
    ASSERT_TRUE(string_equals(result, STRING_LIT("x=7, y=3.14")));
    growing_arena_destroy(a);
}

/* --- string_replace ----------------------------------------------------- */

TEST(string_replace_basic)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    string_t r = string_replace(
        STRING_LIT("hello world world"),
        STRING_LIT("world"),
        STRING_LIT("there"),
        growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(r, STRING_LIT("hello there there")));
    growing_arena_destroy(a);
}

TEST(string_replace_no_match)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    string_t r = string_replace(
        STRING_LIT("hello"),
        STRING_LIT("xyz"),
        STRING_LIT("!"),
        growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(r, STRING_LIT("hello")));
    growing_arena_destroy(a);
}

TEST(string_replace_empty_needle_returns_copy)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    string_t r = string_replace(
        STRING_LIT("hello"),
        STRING_LIT(""),
        STRING_LIT("X"),
        growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(r, STRING_LIT("hello")));
    growing_arena_destroy(a);
}

TEST(string_replace_whole_string)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    string_t r = string_replace(
        STRING_LIT("aaa"),
        STRING_LIT("a"),
        STRING_LIT("bb"),
        growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(r, STRING_LIT("bbbbbb")));
    growing_arena_destroy(a);
}

TEST(string_replace_with_empty_replacement)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    string_t r = string_replace(
        STRING_LIT("a,b,c"),
        STRING_LIT(","),
        STRING_LIT(""),
        growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(r, STRING_LIT("abc")));
    growing_arena_destroy(a);
}

/* --- string_to_uppercase / string_to_lowercase -------------------------- */

TEST(string_to_uppercase_basic)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    string_t r = string_to_uppercase(
        STRING_LIT("Hello World!"), growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(r, STRING_LIT("HELLO WORLD!")));
    growing_arena_destroy(a);
}

TEST(string_to_lowercase_basic)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    string_t r = string_to_lowercase(
        STRING_LIT("Hello World!"), growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(r, STRING_LIT("hello world!")));
    growing_arena_destroy(a);
}

TEST(string_to_uppercase_already_upper)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    string_t r =
        string_to_uppercase(STRING_LIT("ABC"), growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(r, STRING_LIT("ABC")));
    growing_arena_destroy(a);
}

TEST(string_to_uppercase_empty)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    string_t r =
        string_to_uppercase((string_t){NULL, 0}, growing_arena_allocator(a));
    ASSERT_EQ(0, r.len);
    growing_arena_destroy(a);
}

/* --- string_join -------------------------------------------------------- */

TEST(string_join_basic)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 512);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = string_split_substr(
        STRING_LIT("a,b,c"), STRING_LIT(","), scratch_allocator(&sc));
    string_t result =
        string_join(it, STRING_LIT("-"), growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(result, STRING_LIT("a-b-c")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_join_single_token)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = string_split_substr(
        STRING_LIT("hello"), STRING_LIT(","), scratch_allocator(&sc));
    string_t result =
        string_join(it, STRING_LIT(","), growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(result, STRING_LIT("hello")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string_join_empty_separator)
{
    growing_arena_t _a_storage;
    growing_arena_t *a = &_a_storage;
    growing_arena_init(a, 256);
    scratch_t sc;
    growing_arena_scratch_begin(&sc, a);
    iter_t it = string_split_substr(
        STRING_LIT("a,b,c"), STRING_LIT(","), scratch_allocator(&sc));
    string_t result =
        string_join(it, STRING_LIT(""), growing_arena_allocator(a));
    ASSERT_TRUE(string_equals(result, STRING_LIT("abc")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* --- string_to_int ------------------------------------------------------ */

TEST(string_to_int_positive)
{
    long long val;
    ASSERT_TRUE(string_to_int(STRING_LIT("42"), &val));
    ASSERT_EQ(42, val);
}

TEST(string_to_int_negative)
{
    long long val;
    ASSERT_TRUE(string_to_int(STRING_LIT("-7"), &val));
    ASSERT_EQ(-7, val);
}

TEST(string_to_int_zero)
{
    long long val;
    ASSERT_TRUE(string_to_int(STRING_LIT("0"), &val));
    ASSERT_EQ(0, val);
}

TEST(string_to_int_empty)
{
    long long val;
    ASSERT_FALSE(string_to_int(STRING_LIT(""), &val));
}

TEST(string_to_int_invalid_alpha)
{
    long long val;
    ASSERT_FALSE(string_to_int(STRING_LIT("abc"), &val));
}

TEST(string_to_int_trailing_garbage)
{
    long long val;
    ASSERT_FALSE(string_to_int(STRING_LIT("42abc"), &val));
}

/* --- string_to_double --------------------------------------------------- */

TEST(string_to_double_positive)
{
    double val;
    ASSERT_TRUE(string_to_double(STRING_LIT("3.14"), &val));
    ASSERT_FLOAT_EQ(3.14, val, 1e-9);
}

TEST(string_to_double_negative)
{
    double val;
    ASSERT_TRUE(string_to_double(STRING_LIT("-2.5"), &val));
    ASSERT_FLOAT_EQ(-2.5, val, 1e-9);
}

TEST(string_to_double_integer_value)
{
    double val;
    ASSERT_TRUE(string_to_double(STRING_LIT("42"), &val));
    ASSERT_FLOAT_EQ(42.0, val, 1e-9);
}

TEST(string_to_double_scientific)
{
    double val;
    ASSERT_TRUE(string_to_double(STRING_LIT("1.5e2"), &val));
    ASSERT_FLOAT_EQ(150.0, val, 1e-9);
}

TEST(string_to_double_empty)
{
    double val;
    ASSERT_FALSE(string_to_double(STRING_LIT(""), &val));
}

TEST(string_to_double_invalid)
{
    double val;
    ASSERT_FALSE(string_to_double(STRING_LIT("abc"), &val));
}

TEST(string_to_double_trailing_garbage)
{
    double val;
    ASSERT_FALSE(string_to_double(STRING_LIT("1.5x"), &val));
}

/* --- OOM paths ---------------------------------------------------------- */

TEST(string_copy_oom_returns_empty)
{
    string_t c = string_copy(STRING_LIT("hello"), null_allocator());
    ASSERT_NULL(c.ptr);
    ASSERT_EQ(0u, c.len);
}

TEST(string_split_oom_returns_empty)
{
    oom_ctx_t ctx;
    allocator_t al = oom_after_allocator(0, &ctx); /* state alloc fails */
    iter_t it = string_split_substr(STRING_LIT("a,b,c"), STRING_LIT(","), al);
    ASSERT_NULL(it.next); /* empty iterator, not a NULL deref */
    iter_destroy(&it);
}

/* --- printf interop (STRING_FMT / STRING_ARG) --------------------------- */

TEST(string_fmt_macro_prints_view)
{
    char buf[64];
    int n = snprintf(
        buf, sizeof buf, "<" STRING_FMT ">", STRING_ARG(STRING_LIT("hello")));
    ASSERT_EQ(7, n);
    ASSERT_STR_EQ("<hello>", buf);
}

TEST(string_fmt_macro_respects_length_not_nul)
{
    /* A non-NUL-terminated slice into the middle of a larger buffer: the
     * precision form must print exactly s.len bytes, not run to a NUL. */
    string_t s = string_slice(STRING_LIT("hello world"), 0, 5);
    char buf[16];
    int n = snprintf(buf, sizeof buf, STRING_FMT, STRING_ARG(s));
    ASSERT_EQ(5, n);
    ASSERT_STR_EQ("hello", buf);
}

/* --- string_to_cstr_buf ------------------------------------------------- */

TEST(string_to_cstr_buf_copies_and_terminates)
{
    char buf[16];
    const char *c = string_to_cstr_buf(STRING_LIT("hi"), buf, sizeof buf);
    ASSERT_PTR_EQ(buf, c);
    ASSERT_STR_EQ("hi", c);
}

TEST(string_to_cstr_buf_exact_fit)
{
    char buf[4]; /* "abc" + NUL == 4 bytes */
    const char *c = string_to_cstr_buf(STRING_LIT("abc"), buf, sizeof buf);
    ASSERT_NOT_NULL(c);
    ASSERT_STR_EQ("abc", c);
}

TEST(string_to_cstr_buf_rejects_too_small)
{
    char buf[3]; /* "abc" needs 4 bytes; 3 is too small */
    ASSERT_NULL(string_to_cstr_buf(STRING_LIT("abc"), buf, sizeof buf));
}

TEST(string_to_cstr_buf_empty_string)
{
    char buf[4];
    const char *c = string_to_cstr_buf((string_t){NULL, 0}, buf, sizeof buf);
    ASSERT_NOT_NULL(c);
    ASSERT_STR_EQ("", c);
}

TEST(string_to_cstr_buf_null_buf_or_zero_size)
{
    char buf[4];
    ASSERT_NULL(string_to_cstr_buf(STRING_LIT("a"), NULL, 4));
    ASSERT_NULL(string_to_cstr_buf(STRING_LIT("a"), buf, 0));
}

/* --- Results are plain allocations ----------------------------------------
 *
 * Every function that returns a new string returns a plain allocation of
 * exactly len bytes, freed with mem_free(alloc, ptr, len) — not a view into
 * some internal buffer.  These use the malloc-based sys_allocator, so
 * ./test.sh asan catches a wrong pointer (free of a non-malloc'd address),
 * a wrong size, or anything left behind (LeakSanitizer). */

static void free_string(allocator_t alloc, string_t s)
{
    mem_free(alloc, (void *)s.ptr, s.len);
}

TEST(string_strbuf_to_string_is_an_owned_copy)
{
    allocator_t heap = sys_allocator();
    strbuf_t *sb = strbuf_create(heap);
    strbuf_append(sb, STRING_LIT("hello"));
    string_t out = strbuf_to_string(sb, heap);
    strbuf_append(sb, STRING_LIT(" world, long enough to move the buffer"));
    strbuf_destroy(sb);
    ASSERT_TRUE(string_equals(out, STRING_LIT("hello")));
    free_string(heap, out);
}

TEST(string_strbuf_to_string_empty_and_null)
{
    allocator_t heap = sys_allocator();
    strbuf_t *sb = strbuf_create(heap);
    string_t out = strbuf_to_string(sb, heap);
    ASSERT_NULL(out.ptr);
    ASSERT_EQ(0u, out.len);
    strbuf_destroy(sb);
    ASSERT_EQ(0u, strbuf_to_string(NULL, heap).len);
    ASSERT_EQ(0u, strbuf_view(NULL).len);
}

TEST(string_strbuf_to_string_oom)
{
    allocator_t heap = sys_allocator();
    strbuf_t *sb = strbuf_create(heap);
    strbuf_append(sb, STRING_LIT("hello"));
    string_t out = strbuf_to_string(sb, ALLOCATOR_NULL);
    ASSERT_NULL(out.ptr);
    ASSERT_EQ(0u, out.len);
    strbuf_destroy(sb);
}

TEST(string_strbuf_finish_is_strbuf_view)
{
    allocator_t heap = sys_allocator();
    strbuf_t *sb = strbuf_create(heap);
    strbuf_append(sb, STRING_LIT("abc"));
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    string_t old = strbuf_finish(sb);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
    string_t view = strbuf_view(sb);
    ASSERT_TRUE(old.ptr == view.ptr && old.len == view.len);
    strbuf_destroy(sb);
}

TEST(string_replace_result_is_plain_allocation)
{
    allocator_t heap = sys_allocator();
    string_t r = string_replace(
        STRING_LIT("a.b.c"), STRING_LIT("."), STRING_LIT("::"), heap);
    ASSERT_TRUE(string_equals(r, STRING_LIT("a::b::c")));
    free_string(heap, r);

    r = string_replace(
        STRING_LIT("abc"), STRING_LIT(""), STRING_LIT("x"), heap);
    ASSERT_TRUE(string_equals(r, STRING_LIT("abc")));
    free_string(heap, r);

    r = string_replace(
        STRING_LIT("aaa"), STRING_LIT("a"), STRING_LIT(""), heap);
    ASSERT_EQ(0u, r.len);
    free_string(heap, r);
}

TEST(string_replace_oom_returns_empty)
{
    for (size_t n = 0; n < 8; ++n)
    {
        oom_ctx_t ctx = {0};
        allocator_t al = oom_after_allocator(n, &ctx);
        string_t r = string_replace(
            STRING_LIT("a.b.c"), STRING_LIT("."), STRING_LIT("::"), al);
        if (r.ptr)
        {
            ASSERT_TRUE(string_equals(r, STRING_LIT("a::b::c")));
            free_string(al, r);
        }
        else
        {
            ASSERT_EQ(0u, r.len);
        }
    }
}

TEST(string_join_result_is_plain_allocation)
{
    allocator_t heap = sys_allocator();
    iter_t it = string_split_substr(STRING_LIT("a,b,c"), STRING_LIT(","), heap);
    string_t r = string_join(it, STRING_LIT(" - "), heap);
    ASSERT_TRUE(string_equals(r, STRING_LIT("a - b - c")));
    free_string(heap, r);
}

TEST(string_join_oom_returns_empty)
{
    allocator_t heap = sys_allocator();
    for (size_t n = 0; n < 8; ++n)
    {
        oom_ctx_t ctx = {0};
        allocator_t al = oom_after_allocator(n, &ctx);
        iter_t it =
            string_split_substr(STRING_LIT("a,b,c"), STRING_LIT(","), heap);
        string_t r = string_join(it, STRING_LIT("-"), al);
        if (r.ptr)
        {
            ASSERT_TRUE(string_equals(r, STRING_LIT("a-b-c")));
            free_string(al, r);
        }
        else
        {
            ASSERT_EQ(0u, r.len);
        }
    }
}

TEST(string_copies_are_plain_allocations)
{
    allocator_t heap = sys_allocator();
    string_t c = string_copy(STRING_LIT("Mixed"), heap);
    string_t f = string_from_cstr("Mixed", heap);
    string_t u = string_to_uppercase(STRING_LIT("Mixed"), heap);
    string_t l = string_to_lowercase(STRING_LIT("Mixed"), heap);
    ASSERT_TRUE(string_equals(u, STRING_LIT("MIXED")));
    ASSERT_TRUE(string_equals(l, STRING_LIT("mixed")));
    free_string(heap, c);
    free_string(heap, f);
    free_string(heap, u);
    free_string(heap, l);
    const char *z = string_to_cstr(STRING_LIT("Mixed"), heap);
    mem_free(heap, (void *)z, 6); /* len + the NUL */
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "string_test");
}
