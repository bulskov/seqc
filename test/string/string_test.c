#include <gtest/gtest.h>

extern "C" {
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/hashmap.h"
#include "seqc/string.h"
}

/* --- Construction ------------------------------------------------------- */

TEST(string, from_cstr_length)
{
    String s = string_view_cstr("hello");
    EXPECT_EQ(s.len, 5);
}

TEST(string, from_cstr_copies_content)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    char buf[] = "hello";
    String s = string_from_cstr(buf, growing_arena_allocator(a));
    EXPECT_EQ(s.len, 5);
    buf[0] = 'X'; /* mutate source */
    EXPECT_EQ(s.ptr[0], 'h'); /* copy unaffected */
    growing_arena_destroy(a);
}

TEST(string, from_lit_macro)
{
    String s = STRING_LIT("world");
    EXPECT_EQ(s.len, 5);
}

TEST(string, to_cstr_is_null_terminated)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String s = STRING_LIT("hi");
    const char *cs = string_to_cstr(s, growing_arena_allocator(a));
    EXPECT_EQ(cs[2], '\0');
    EXPECT_STREQ(cs, "hi");
    growing_arena_destroy(a);
}

TEST(string, copy_is_independent)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    char buf[] = "mutable";
    String s = string_view_cstr(buf);
    String c = string_copy(s, growing_arena_allocator(a));
    buf[0] = 'X';
    EXPECT_EQ(c.ptr[0], 'm'); /* copy unaffected */
    growing_arena_destroy(a);
}

/* --- Comparison --------------------------------------------------------- */

TEST(string, equals_same_content)
{
    EXPECT_TRUE(string_equals(STRING_LIT("abc"), STRING_LIT("abc")));
}

TEST(string, equals_different_content)
{
    EXPECT_FALSE(string_equals(STRING_LIT("abc"), STRING_LIT("abd")));
}

TEST(string, equals_different_length)
{
    EXPECT_FALSE(string_equals(STRING_LIT("abc"), STRING_LIT("ab")));
}

TEST(string, compare_ordering)
{
    EXPECT_LT(string_compare(STRING_LIT("abc"), STRING_LIT("abd")), 0);
    EXPECT_GT(string_compare(STRING_LIT("b"), STRING_LIT("a")), 0);
    EXPECT_EQ(string_compare(STRING_LIT("x"), STRING_LIT("x")), 0);
}

/* --- Query -------------------------------------------------------------- */

TEST(string, starts_with_true)
{
    EXPECT_TRUE(string_starts_with(STRING_LIT("foobar"), STRING_LIT("foo")));
}

TEST(string, starts_with_false)
{
    EXPECT_FALSE(string_starts_with(STRING_LIT("foobar"), STRING_LIT("bar")));
}

TEST(string, ends_with_true)
{
    EXPECT_TRUE(string_ends_with(STRING_LIT("foobar"), STRING_LIT("bar")));
}

TEST(string, ends_with_false)
{
    EXPECT_FALSE(string_ends_with(STRING_LIT("foobar"), STRING_LIT("foo")));
}

TEST(string, contains_true)
{
    EXPECT_TRUE(string_contains(STRING_LIT("hello world"), STRING_LIT("world")));
}

TEST(string, contains_false)
{
    EXPECT_FALSE(string_contains(STRING_LIT("hello"), STRING_LIT("xyz")));
}

TEST(string, find_returns_index)
{
    EXPECT_EQ(string_find(STRING_LIT("abcdef"), STRING_LIT("cd")), 2);
}

TEST(string, find_not_found)
{
    EXPECT_EQ(
        string_find(STRING_LIT("abc"), STRING_LIT("z")), STRING_NOT_FOUND);
}

/* --- Views --------------------------------------------------------------- */

TEST(string, slice_zero_copy)
{
    String s = STRING_LIT("hello world");
    String sub = string_slice(s, 6, 11);
    EXPECT_EQ(sub.len, 5);
    EXPECT_TRUE(string_equals(sub, STRING_LIT("world")));
    EXPECT_EQ(sub.ptr, s.ptr + 6); /* same pointer — no copy */
}

TEST(string, trim_removes_whitespace)
{
    EXPECT_TRUE(string_equals(
        string_trim(STRING_LIT("  hello  ")), STRING_LIT("hello")));
}

TEST(string, trim_left_only)
{
    String t = string_trim_left(STRING_LIT("  hi"));
    EXPECT_TRUE(string_equals(t, STRING_LIT("hi")));
}

TEST(string, trim_right_only)
{
    String t = string_trim_right(STRING_LIT("hi  "));
    EXPECT_TRUE(string_equals(t, STRING_LIT("hi")));
}

TEST(string, trim_no_whitespace)
{
    EXPECT_TRUE(string_equals(string_trim(STRING_LIT("abc")), STRING_LIT("abc")));
}

/* --- Builder ------------------------------------------------------------ */

TEST(string, builder_append_str)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append(sb, STRING_LIT("hello"));
    sb_append_char(sb, ' ');
    sb_append_cstr(sb, "world");
    String result = sb_finish(sb);
    EXPECT_TRUE(string_equals(result, STRING_LIT("hello world")));
    growing_arena_destroy(a);
}

TEST(string, builder_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    String result = sb_finish(sb);
    EXPECT_EQ(result.len, 0);
    growing_arena_destroy(a);
}

/* --- Iter sources ------------------------------------------------------- */

TEST(string, chars_count)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    size_t n =
        iter_count(string_chars(STRING_LIT("hello"), scratch_allocator(&sc)));
    EXPECT_EQ(n, 5);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string, chars_rev)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = string_chars_rev(STRING_LIT("abc"), scratch_allocator(&sc));
    char c;
    it.next(&it, &c);
    EXPECT_EQ(c, 'c');
    it.next(&it, &c);
    EXPECT_EQ(c, 'b');
    it.next(&it, &c);
    EXPECT_EQ(c, 'a');
    EXPECT_FALSE(it.next(&it, &c));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string, split_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    String parts[3];
    size_t i = 0;
    Iter it = string_split(
        STRING_LIT("a,b,c"), STRING_LIT(","), scratch_allocator(&sc));
    while (it.next(&it, &parts[i]))
        i++;
    iter_drop(&it);
    EXPECT_EQ(i, 3);
    EXPECT_TRUE(string_equals(parts[0], STRING_LIT("a")));
    EXPECT_TRUE(string_equals(parts[1], STRING_LIT("b")));
    EXPECT_TRUE(string_equals(parts[2], STRING_LIT("c")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string, split_trailing_delim)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(string_split(
        STRING_LIT("a,b,"), STRING_LIT(","), scratch_allocator(&sc)));
    EXPECT_EQ(n, 3); /* "a", "b", "" */
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string, split_no_delim)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    String token;
    Iter it = string_split(
        STRING_LIT("hello"), STRING_LIT(","), scratch_allocator(&sc));
    EXPECT_TRUE(it.next(&it, &token));
    EXPECT_TRUE(string_equals(token, STRING_LIT("hello")));
    EXPECT_FALSE(it.next(&it, &token));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* --- HashMap with String keys ------------------------------------------- */

TEST(string, hashmap_string_keys)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 1024);
    HashMap *map = hashmap_create(
        sizeof(String),
        sizeof(int),
        string_hash,
        string_key_eq,
        growing_arena_allocator(a));

    String k1 = STRING_LIT("foo");
    String k2 = STRING_LIT("bar");
    int v1 = 1, v2 = 2;
    hashmap_set(map, &k1, &v1);
    hashmap_set(map, &k2, &v2);

    int g1, g2;
    EXPECT_EQ(hashmap_get(map, &k1, &g1), SEQC_OK); EXPECT_EQ(g1, 1);
    EXPECT_EQ(hashmap_get(map, &k2, &g2), SEQC_OK); EXPECT_EQ(g2, 2);

    /* Key from different pointer but same content must still hit */
    char buf[] = "foo";
    String k1_copy = string_view_cstr(buf);
    int g1c;
    EXPECT_EQ(hashmap_get(map, &k1_copy, &g1c), SEQC_OK); EXPECT_EQ(g1c, 1);

    hashmap_free(map);
    growing_arena_destroy(a);
}

/* --- sb_append_int / sb_append_fmt ------------------------------------- */

TEST(string, sb_append_int_positive)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_int(sb, 42);
    String result = sb_finish(sb);
    EXPECT_TRUE(string_equals(result, STRING_LIT("42")));
    growing_arena_destroy(a);
}

TEST(string, sb_append_int_negative)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_int(sb, -123);
    String result = sb_finish(sb);
    EXPECT_TRUE(string_equals(result, STRING_LIT("-123")));
    growing_arena_destroy(a);
}

TEST(string, sb_append_int_zero)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_int(sb, 0);
    String result = sb_finish(sb);
    EXPECT_TRUE(string_equals(result, STRING_LIT("0")));
    growing_arena_destroy(a);
}

TEST(string, sb_append_fmt_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_fmt(sb, "hello %s, you are %d years old", "world", 30);
    String result = sb_finish(sb);
    EXPECT_TRUE(
        string_equals(result, STRING_LIT("hello world, you are 30 years old")));
    growing_arena_destroy(a);
}

TEST(string, sb_append_fmt_compose)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_cstr(sb, "x=");
    sb_append_fmt(sb, "%d", 7);
    sb_append_cstr(sb, ", y=");
    sb_append_fmt(sb, "%.2f", 3.14);
    String result = sb_finish(sb);
    EXPECT_TRUE(string_equals(result, STRING_LIT("x=7, y=3.14")));
    growing_arena_destroy(a);
}

/* --- string_replace ----------------------------------------------------- */

TEST(string, replace_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    String r = string_replace(
        STRING_LIT("hello world world"),
        STRING_LIT("world"),
        STRING_LIT("there"),
        growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(r, STRING_LIT("hello there there")));
    growing_arena_destroy(a);
}

TEST(string, replace_no_match)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_replace(
        STRING_LIT("hello"),
        STRING_LIT("xyz"),
        STRING_LIT("!"),
        growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(r, STRING_LIT("hello")));
    growing_arena_destroy(a);
}

TEST(string, replace_empty_needle_returns_copy)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_replace(
        STRING_LIT("hello"),
        STRING_LIT(""),
        STRING_LIT("X"),
        growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(r, STRING_LIT("hello")));
    growing_arena_destroy(a);
}

TEST(string, replace_whole_string)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_replace(
        STRING_LIT("aaa"),
        STRING_LIT("a"),
        STRING_LIT("bb"),
        growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(r, STRING_LIT("bbbbbb")));
    growing_arena_destroy(a);
}

TEST(string, replace_with_empty_replacement)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_replace(
        STRING_LIT("a,b,c"),
        STRING_LIT(","),
        STRING_LIT(""),
        growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(r, STRING_LIT("abc")));
    growing_arena_destroy(a);
}

/* --- string_to_uppercase / string_to_lowercase -------------------------- */

TEST(string, to_uppercase_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r =
        string_to_uppercase(STRING_LIT("Hello World!"), growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(r, STRING_LIT("HELLO WORLD!")));
    growing_arena_destroy(a);
}

TEST(string, to_lowercase_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r =
        string_to_lowercase(STRING_LIT("Hello World!"), growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(r, STRING_LIT("hello world!")));
    growing_arena_destroy(a);
}

TEST(string, to_uppercase_already_upper)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_to_uppercase(STRING_LIT("ABC"), growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(r, STRING_LIT("ABC")));
    growing_arena_destroy(a);
}

TEST(string, to_uppercase_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_to_uppercase((String){NULL, 0}, growing_arena_allocator(a));
    EXPECT_EQ(r.len, 0);
    growing_arena_destroy(a);
}

/* --- string_join -------------------------------------------------------- */

TEST(string, join_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = string_split(
        STRING_LIT("a,b,c"), STRING_LIT(","), scratch_allocator(&sc));
    String result = string_join(it, STRING_LIT("-"), growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(result, STRING_LIT("a-b-c")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string, join_single_token)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = string_split(
        STRING_LIT("hello"), STRING_LIT(","), scratch_allocator(&sc));
    String result = string_join(it, STRING_LIT(","), growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(result, STRING_LIT("hello")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

TEST(string, join_empty_separator)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = string_split(
        STRING_LIT("a,b,c"), STRING_LIT(","), scratch_allocator(&sc));
    String result = string_join(it, STRING_LIT(""), growing_arena_allocator(a));
    EXPECT_TRUE(string_equals(result, STRING_LIT("abc")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* --- string_to_int ------------------------------------------------------ */

TEST(string, to_int_positive)
{
    long long val;
    EXPECT_TRUE(string_to_int(STRING_LIT("42"), &val));
    EXPECT_EQ(val, 42);
}

TEST(string, to_int_negative)
{
    long long val;
    EXPECT_TRUE(string_to_int(STRING_LIT("-7"), &val));
    EXPECT_EQ(val, -7);
}

TEST(string, to_int_zero)
{
    long long val;
    EXPECT_TRUE(string_to_int(STRING_LIT("0"), &val));
    EXPECT_EQ(val, 0);
}

TEST(string, to_int_empty)
{
    long long val;
    EXPECT_FALSE(string_to_int(STRING_LIT(""), &val));
}

TEST(string, to_int_invalid_alpha)
{
    long long val;
    EXPECT_FALSE(string_to_int(STRING_LIT("abc"), &val));
}

TEST(string, to_int_trailing_garbage)
{
    long long val;
    EXPECT_FALSE(string_to_int(STRING_LIT("42abc"), &val));
}

/* --- string_to_double --------------------------------------------------- */

TEST(string, to_double_positive)
{
    double val;
    EXPECT_TRUE(string_to_double(STRING_LIT("3.14"), &val));
    EXPECT_NEAR(val, 3.14, 1e-9);
}

TEST(string, to_double_negative)
{
    double val;
    EXPECT_TRUE(string_to_double(STRING_LIT("-2.5"), &val));
    EXPECT_NEAR(val, -2.5, 1e-9);
}

TEST(string, to_double_integer_value)
{
    double val;
    EXPECT_TRUE(string_to_double(STRING_LIT("42"), &val));
    EXPECT_NEAR(val, 42.0, 1e-9);
}

TEST(string, to_double_scientific)
{
    double val;
    EXPECT_TRUE(string_to_double(STRING_LIT("1.5e2"), &val));
    EXPECT_NEAR(val, 150.0, 1e-9);
}

TEST(string, to_double_empty)
{
    double val;
    EXPECT_FALSE(string_to_double(STRING_LIT(""), &val));
}

TEST(string, to_double_invalid)
{
    double val;
    EXPECT_FALSE(string_to_double(STRING_LIT("abc"), &val));
}

TEST(string, to_double_trailing_garbage)
{
    double val;
    EXPECT_FALSE(string_to_double(STRING_LIT("1.5x"), &val));
}
