#include <criterion/criterion.h>

#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/hashmap.h"
#include "seqc/string.h"

/* --- Construction ------------------------------------------------------- */

Test(string, from_cstr_length)
{
    String s = string_view_cstr("hello");
    cr_assert_eq(s.len, 5);
}

Test(string, from_cstr_copies_content)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    char buf[] = "hello";
    String s = string_from_cstr(buf, growing_arena_allocator(a));
    cr_assert_eq(s.len, 5);
    buf[0] = 'X'; /* mutate source */
    cr_assert_eq(s.ptr[0], 'h'); /* copy unaffected */
    growing_arena_destroy(a);
}

Test(string, from_lit_macro)
{
    String s = STRING_LIT("world");
    cr_assert_eq(s.len, 5);
}

Test(string, to_cstr_is_null_terminated)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String s = STRING_LIT("hi");
    const char *cs = string_to_cstr(s, growing_arena_allocator(a));
    cr_assert_eq(cs[2], '\0');
    cr_assert_str_eq(cs, "hi");
    growing_arena_destroy(a);
}

Test(string, copy_is_independent)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    char buf[] = "mutable";
    String s = string_view_cstr(buf);
    String c = string_copy(s, growing_arena_allocator(a));
    buf[0] = 'X';
    cr_assert_eq(c.ptr[0], 'm'); /* copy unaffected */
    growing_arena_destroy(a);
}

/* --- Comparison --------------------------------------------------------- */

Test(string, equals_same_content)
{
    cr_assert(string_equals(STRING_LIT("abc"), STRING_LIT("abc")));
}

Test(string, equals_different_content)
{
    cr_assert_not(string_equals(STRING_LIT("abc"), STRING_LIT("abd")));
}

Test(string, equals_different_length)
{
    cr_assert_not(string_equals(STRING_LIT("abc"), STRING_LIT("ab")));
}

Test(string, compare_ordering)
{
    cr_assert_lt(string_compare(STRING_LIT("abc"), STRING_LIT("abd")), 0);
    cr_assert_gt(string_compare(STRING_LIT("b"), STRING_LIT("a")), 0);
    cr_assert_eq(string_compare(STRING_LIT("x"), STRING_LIT("x")), 0);
}

/* --- Query -------------------------------------------------------------- */

Test(string, starts_with_true)
{
    cr_assert(string_starts_with(STRING_LIT("foobar"), STRING_LIT("foo")));
}

Test(string, starts_with_false)
{
    cr_assert_not(string_starts_with(STRING_LIT("foobar"), STRING_LIT("bar")));
}

Test(string, ends_with_true)
{
    cr_assert(string_ends_with(STRING_LIT("foobar"), STRING_LIT("bar")));
}

Test(string, ends_with_false)
{
    cr_assert_not(string_ends_with(STRING_LIT("foobar"), STRING_LIT("foo")));
}

Test(string, contains_true)
{
    cr_assert(string_contains(STRING_LIT("hello world"), STRING_LIT("world")));
}

Test(string, contains_false)
{
    cr_assert_not(string_contains(STRING_LIT("hello"), STRING_LIT("xyz")));
}

Test(string, find_returns_index)
{
    cr_assert_eq(string_find(STRING_LIT("abcdef"), STRING_LIT("cd")), 2);
}

Test(string, find_not_found)
{
    cr_assert_eq(
        string_find(STRING_LIT("abc"), STRING_LIT("z")), STRING_NOT_FOUND);
}

/* --- Views --------------------------------------------------------------- */

Test(string, slice_zero_copy)
{
    String s = STRING_LIT("hello world");
    String sub = string_slice(s, 6, 11);
    cr_assert_eq(sub.len, 5);
    cr_assert(string_equals(sub, STRING_LIT("world")));
    cr_assert_eq(sub.ptr, s.ptr + 6); /* same pointer — no copy */
}

Test(string, trim_removes_whitespace)
{
    cr_assert(string_equals(
        string_trim(STRING_LIT("  hello  ")), STRING_LIT("hello")));
}

Test(string, trim_left_only)
{
    String t = string_trim_left(STRING_LIT("  hi"));
    cr_assert(string_equals(t, STRING_LIT("hi")));
}

Test(string, trim_right_only)
{
    String t = string_trim_right(STRING_LIT("hi  "));
    cr_assert(string_equals(t, STRING_LIT("hi")));
}

Test(string, trim_no_whitespace)
{
    cr_assert(string_equals(string_trim(STRING_LIT("abc")), STRING_LIT("abc")));
}

/* --- Builder ------------------------------------------------------------ */

Test(string, builder_append_str)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append(sb, STRING_LIT("hello"));
    sb_append_char(sb, ' ');
    sb_append_cstr(sb, "world");
    String result = sb_finish(sb);
    cr_assert(string_equals(result, STRING_LIT("hello world")));
    growing_arena_destroy(a);
}

Test(string, builder_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    String result = sb_finish(sb);
    cr_assert_eq(result.len, 0);
    growing_arena_destroy(a);
}

/* --- Iter sources ------------------------------------------------------- */

Test(string, chars_count)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    size_t n =
        iter_count(string_chars(STRING_LIT("hello"), scratch_allocator(&sc)));
    cr_assert_eq(n, 5);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(string, chars_rev)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = string_chars_rev(STRING_LIT("abc"), scratch_allocator(&sc));
    char c;
    it.next(&it, &c);
    cr_assert_eq(c, 'c');
    it.next(&it, &c);
    cr_assert_eq(c, 'b');
    it.next(&it, &c);
    cr_assert_eq(c, 'a');
    cr_assert_not(it.next(&it, &c));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(string, split_basic)
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
    cr_assert_eq(i, 3);
    cr_assert(string_equals(parts[0], STRING_LIT("a")));
    cr_assert(string_equals(parts[1], STRING_LIT("b")));
    cr_assert(string_equals(parts[2], STRING_LIT("c")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(string, split_trailing_delim)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    size_t n = iter_count(string_split(
        STRING_LIT("a,b,"), STRING_LIT(","), scratch_allocator(&sc)));
    cr_assert_eq(n, 3); /* "a", "b", "" */
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(string, split_no_delim)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    String token;
    Iter it = string_split(
        STRING_LIT("hello"), STRING_LIT(","), scratch_allocator(&sc));
    cr_assert(it.next(&it, &token));
    cr_assert(string_equals(token, STRING_LIT("hello")));
    cr_assert_not(it.next(&it, &token));
    iter_drop(&it);
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* --- HashMap with String keys ------------------------------------------- */

Test(string, hashmap_string_keys)
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
    cr_assert_eq(hashmap_get(map, &k1, &g1), SEQC_OK); cr_assert_eq(g1, 1);
    cr_assert_eq(hashmap_get(map, &k2, &g2), SEQC_OK); cr_assert_eq(g2, 2);

    /* Key from different pointer but same content must still hit */
    char buf[] = "foo";
    String k1_copy = string_view_cstr(buf);
    int g1c;
    cr_assert_eq(hashmap_get(map, &k1_copy, &g1c), SEQC_OK); cr_assert_eq(g1c, 1);

    hashmap_free(map);
    growing_arena_destroy(a);
}

/* --- sb_append_int / sb_append_fmt ------------------------------------- */

Test(string, sb_append_int_positive)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_int(sb, 42);
    String result = sb_finish(sb);
    cr_assert(string_equals(result, STRING_LIT("42")));
    growing_arena_destroy(a);
}

Test(string, sb_append_int_negative)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_int(sb, -123);
    String result = sb_finish(sb);
    cr_assert(string_equals(result, STRING_LIT("-123")));
    growing_arena_destroy(a);
}

Test(string, sb_append_int_zero)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_int(sb, 0);
    String result = sb_finish(sb);
    cr_assert(string_equals(result, STRING_LIT("0")));
    growing_arena_destroy(a);
}

Test(string, sb_append_fmt_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_fmt(sb, "hello %s, you are %d years old", "world", 30);
    String result = sb_finish(sb);
    cr_assert(
        string_equals(result, STRING_LIT("hello world, you are 30 years old")));
    growing_arena_destroy(a);
}

Test(string, sb_append_fmt_compose)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    StringBuilder *sb = sb_create(growing_arena_allocator(a));
    sb_append_cstr(sb, "x=");
    sb_append_fmt(sb, "%d", 7);
    sb_append_cstr(sb, ", y=");
    sb_append_fmt(sb, "%.2f", 3.14);
    String result = sb_finish(sb);
    cr_assert(string_equals(result, STRING_LIT("x=7, y=3.14")));
    growing_arena_destroy(a);
}

/* --- string_replace ----------------------------------------------------- */

Test(string, replace_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    String r = string_replace(
        STRING_LIT("hello world world"),
        STRING_LIT("world"),
        STRING_LIT("there"),
        growing_arena_allocator(a));
    cr_assert(string_equals(r, STRING_LIT("hello there there")));
    growing_arena_destroy(a);
}

Test(string, replace_no_match)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_replace(
        STRING_LIT("hello"),
        STRING_LIT("xyz"),
        STRING_LIT("!"),
        growing_arena_allocator(a));
    cr_assert(string_equals(r, STRING_LIT("hello")));
    growing_arena_destroy(a);
}

Test(string, replace_empty_needle_returns_copy)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_replace(
        STRING_LIT("hello"),
        STRING_LIT(""),
        STRING_LIT("X"),
        growing_arena_allocator(a));
    cr_assert(string_equals(r, STRING_LIT("hello")));
    growing_arena_destroy(a);
}

Test(string, replace_whole_string)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_replace(
        STRING_LIT("aaa"),
        STRING_LIT("a"),
        STRING_LIT("bb"),
        growing_arena_allocator(a));
    cr_assert(string_equals(r, STRING_LIT("bbbbbb")));
    growing_arena_destroy(a);
}

Test(string, replace_with_empty_replacement)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_replace(
        STRING_LIT("a,b,c"),
        STRING_LIT(","),
        STRING_LIT(""),
        growing_arena_allocator(a));
    cr_assert(string_equals(r, STRING_LIT("abc")));
    growing_arena_destroy(a);
}

/* --- string_to_uppercase / string_to_lowercase -------------------------- */

Test(string, to_uppercase_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r =
        string_to_uppercase(STRING_LIT("Hello World!"), growing_arena_allocator(a));
    cr_assert(string_equals(r, STRING_LIT("HELLO WORLD!")));
    growing_arena_destroy(a);
}

Test(string, to_lowercase_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r =
        string_to_lowercase(STRING_LIT("Hello World!"), growing_arena_allocator(a));
    cr_assert(string_equals(r, STRING_LIT("hello world!")));
    growing_arena_destroy(a);
}

Test(string, to_uppercase_already_upper)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_to_uppercase(STRING_LIT("ABC"), growing_arena_allocator(a));
    cr_assert(string_equals(r, STRING_LIT("ABC")));
    growing_arena_destroy(a);
}

Test(string, to_uppercase_empty)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    String r = string_to_uppercase((String){NULL, 0}, growing_arena_allocator(a));
    cr_assert_eq(r.len, 0);
    growing_arena_destroy(a);
}

/* --- string_join -------------------------------------------------------- */

Test(string, join_basic)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 512);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = string_split(
        STRING_LIT("a,b,c"), STRING_LIT(","), scratch_allocator(&sc));
    String result = string_join(it, STRING_LIT("-"), growing_arena_allocator(a));
    cr_assert(string_equals(result, STRING_LIT("a-b-c")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(string, join_single_token)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = string_split(
        STRING_LIT("hello"), STRING_LIT(","), scratch_allocator(&sc));
    String result = string_join(it, STRING_LIT(","), growing_arena_allocator(a));
    cr_assert(string_equals(result, STRING_LIT("hello")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

Test(string, join_empty_separator)
{
    growing_arena_t _a_storage; growing_arena_t *a = &_a_storage; growing_arena_init(a, 256);
    scratch_t sc; growing_arena_scratch_begin(&sc, a);
    Iter it = string_split(
        STRING_LIT("a,b,c"), STRING_LIT(","), scratch_allocator(&sc));
    String result = string_join(it, STRING_LIT(""), growing_arena_allocator(a));
    cr_assert(string_equals(result, STRING_LIT("abc")));
    scratch_end(&sc);
    growing_arena_destroy(a);
}

/* --- string_to_int ------------------------------------------------------ */

Test(string, to_int_positive)
{
    long long val;
    cr_assert(string_to_int(STRING_LIT("42"), &val));
    cr_assert_eq(val, 42);
}

Test(string, to_int_negative)
{
    long long val;
    cr_assert(string_to_int(STRING_LIT("-7"), &val));
    cr_assert_eq(val, -7);
}

Test(string, to_int_zero)
{
    long long val;
    cr_assert(string_to_int(STRING_LIT("0"), &val));
    cr_assert_eq(val, 0);
}

Test(string, to_int_empty)
{
    long long val;
    cr_assert_not(string_to_int(STRING_LIT(""), &val));
}

Test(string, to_int_invalid_alpha)
{
    long long val;
    cr_assert_not(string_to_int(STRING_LIT("abc"), &val));
}

Test(string, to_int_trailing_garbage)
{
    long long val;
    cr_assert_not(string_to_int(STRING_LIT("42abc"), &val));
}

/* --- string_to_double --------------------------------------------------- */

Test(string, to_double_positive)
{
    double val;
    cr_assert(string_to_double(STRING_LIT("3.14"), &val));
    cr_assert_float_eq(val, 3.14, 1e-9);
}

Test(string, to_double_negative)
{
    double val;
    cr_assert(string_to_double(STRING_LIT("-2.5"), &val));
    cr_assert_float_eq(val, -2.5, 1e-9);
}

Test(string, to_double_integer_value)
{
    double val;
    cr_assert(string_to_double(STRING_LIT("42"), &val));
    cr_assert_float_eq(val, 42.0, 1e-9);
}

Test(string, to_double_scientific)
{
    double val;
    cr_assert(string_to_double(STRING_LIT("1.5e2"), &val));
    cr_assert_float_eq(val, 150.0, 1e-9);
}

Test(string, to_double_empty)
{
    double val;
    cr_assert_not(string_to_double(STRING_LIT(""), &val));
}

Test(string, to_double_invalid)
{
    double val;
    cr_assert_not(string_to_double(STRING_LIT("abc"), &val));
}

Test(string, to_double_trailing_garbage)
{
    double val;
    cr_assert_not(string_to_double(STRING_LIT("1.5x"), &val));
}
