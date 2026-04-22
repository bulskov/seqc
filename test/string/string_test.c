#include <criterion/criterion.h>

#include "arena/arena.h"
#include "hashmap/hashmap.h"
#include "string/string.h"

/* --- Construction ------------------------------------------------------- */

Test(string, from_cstr_length) {
  String s = string_from_cstr("hello");
  cr_assert_eq(s.len, 5);
}

Test(string, from_lit_macro) {
  String s = STRING_LIT("world");
  cr_assert_eq(s.len, 5);
}

Test(string, to_cstr_is_null_terminated) {
  Arena *a = arena_create(256);
  String s = STRING_LIT("hi");
  const char *cs = string_to_cstr(s, arena_allocator(a));
  cr_assert_eq(cs[2], '\0');
  cr_assert_str_eq(cs, "hi");
  arena_free(a);
}

Test(string, copy_is_independent) {
  Arena *a = arena_create(256);
  char buf[] = "mutable";
  String s = string_from_cstr(buf);
  String c = string_copy(s, arena_allocator(a));
  buf[0] = 'X';
  cr_assert_eq(c.ptr[0], 'm'); /* copy unaffected */
  arena_free(a);
}

/* --- Comparison --------------------------------------------------------- */

Test(string, equals_same_content) {
  cr_assert(string_equals(STRING_LIT("abc"), STRING_LIT("abc")));
}

Test(string, equals_different_content) {
  cr_assert_not(string_equals(STRING_LIT("abc"), STRING_LIT("abd")));
}

Test(string, equals_different_length) {
  cr_assert_not(string_equals(STRING_LIT("abc"), STRING_LIT("ab")));
}

Test(string, compare_ordering) {
  cr_assert_lt(string_compare(STRING_LIT("abc"), STRING_LIT("abd")), 0);
  cr_assert_gt(string_compare(STRING_LIT("b"), STRING_LIT("a")), 0);
  cr_assert_eq(string_compare(STRING_LIT("x"), STRING_LIT("x")), 0);
}

/* --- Query -------------------------------------------------------------- */

Test(string, starts_with_true) {
  cr_assert(string_starts_with(STRING_LIT("foobar"), STRING_LIT("foo")));
}

Test(string, starts_with_false) {
  cr_assert_not(string_starts_with(STRING_LIT("foobar"), STRING_LIT("bar")));
}

Test(string, ends_with_true) {
  cr_assert(string_ends_with(STRING_LIT("foobar"), STRING_LIT("bar")));
}

Test(string, ends_with_false) {
  cr_assert_not(string_ends_with(STRING_LIT("foobar"), STRING_LIT("foo")));
}

Test(string, contains_true) {
  cr_assert(string_contains(STRING_LIT("hello world"), STRING_LIT("world")));
}

Test(string, contains_false) {
  cr_assert_not(string_contains(STRING_LIT("hello"), STRING_LIT("xyz")));
}

Test(string, find_returns_index) {
  cr_assert_eq(string_find(STRING_LIT("abcdef"), STRING_LIT("cd")), 2);
}

Test(string, find_not_found) {
  cr_assert_eq(string_find(STRING_LIT("abc"), STRING_LIT("z")),
               STRING_NOT_FOUND);
}

/* --- Views --------------------------------------------------------------- */

Test(string, slice_zero_copy) {
  String s = STRING_LIT("hello world");
  String sub = string_slice(s, 6, 11);
  cr_assert_eq(sub.len, 5);
  cr_assert(string_equals(sub, STRING_LIT("world")));
  cr_assert_eq(sub.ptr, s.ptr + 6); /* same pointer — no copy */
}

Test(string, trim_removes_whitespace) {
  cr_assert(
      string_equals(string_trim(STRING_LIT("  hello  ")), STRING_LIT("hello")));
}

Test(string, trim_left_only) {
  String t = string_trim_left(STRING_LIT("  hi"));
  cr_assert(string_equals(t, STRING_LIT("hi")));
}

Test(string, trim_right_only) {
  String t = string_trim_right(STRING_LIT("hi  "));
  cr_assert(string_equals(t, STRING_LIT("hi")));
}

Test(string, trim_no_whitespace) {
  cr_assert(string_equals(string_trim(STRING_LIT("abc")), STRING_LIT("abc")));
}

/* --- Builder ------------------------------------------------------------ */

Test(string, builder_append_str) {
  Arena *a = arena_create(256);
  StringBuilder sb = sb_create(arena_allocator(a));
  sb_append(&sb, STRING_LIT("hello"));
  sb_append_char(&sb, ' ');
  sb_append_cstr(&sb, "world");
  String result = sb_finish(&sb);
  cr_assert(string_equals(result, STRING_LIT("hello world")));
  arena_free(a);
}

Test(string, builder_empty) {
  Arena *a = arena_create(256);
  StringBuilder sb = sb_create(arena_allocator(a));
  String result = sb_finish(&sb);
  cr_assert_eq(result.len, 0);
  arena_free(a);
}

/* --- Iter sources ------------------------------------------------------- */

Test(string, chars_count) {
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  size_t n =
      iter_count(string_chars(STRING_LIT("hello"), scratch_allocator(&sc)));
  cr_assert_eq(n, 5);
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(string, chars_rev) {
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
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
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(string, split_basic) {
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  String parts[3];
  size_t i = 0;
  Iter it = string_split(STRING_LIT("a,b,c"), STRING_LIT(","),
                         scratch_allocator(&sc));
  while (it.next(&it, &parts[i]))
    i++;
  iter_drop(&it);
  cr_assert_eq(i, 3);
  cr_assert(string_equals(parts[0], STRING_LIT("a")));
  cr_assert(string_equals(parts[1], STRING_LIT("b")));
  cr_assert(string_equals(parts[2], STRING_LIT("c")));
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(string, split_trailing_delim) {
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  size_t n = iter_count(string_split(STRING_LIT("a,b,"), STRING_LIT(","),
                                     scratch_allocator(&sc)));
  cr_assert_eq(n, 3); /* "a", "b", "" */
  arena_scratch_pop(&sc);
  arena_free(a);
}

Test(string, split_no_delim) {
  Arena *a = arena_create(256);
  Scratch sc = arena_scratch_push(a);
  String token;
  Iter it = string_split(STRING_LIT("hello"), STRING_LIT(","),
                         scratch_allocator(&sc));
  cr_assert(it.next(&it, &token));
  cr_assert(string_equals(token, STRING_LIT("hello")));
  cr_assert_not(it.next(&it, &token));
  iter_drop(&it);
  arena_scratch_pop(&sc);
  arena_free(a);
}

/* --- HashMap with String keys ------------------------------------------- */

Test(string, hashmap_string_keys) {
  Arena *a = arena_create(1024);
  HashMap map = hashmap_create(sizeof(String), sizeof(int), string_hash,
                               string_key_eq, arena_allocator(a));

  String k1 = STRING_LIT("foo");
  String k2 = STRING_LIT("bar");
  int v1 = 1, v2 = 2;
  hashmap_set(&map, &k1, &v1);
  hashmap_set(&map, &k2, &v2);

  cr_assert_eq(*(int *)hashmap_get(&map, &k1), 1);
  cr_assert_eq(*(int *)hashmap_get(&map, &k2), 2);

  /* Key from different pointer but same content must still hit */
  char buf[] = "foo";
  String k1_copy = string_from_cstr(buf);
  cr_assert_eq(*(int *)hashmap_get(&map, &k1_copy), 1);

  hashmap_free(&map);
  arena_free(a);
}

/* --- sb_append_int / sb_append_fmt ------------------------------------- */

Test(string, sb_append_int_positive) {
  Arena *a = arena_create(256);
  StringBuilder sb = sb_create(arena_allocator(a));
  sb_append_int(&sb, 42);
  String result = sb_finish(&sb);
  cr_assert(string_equals(result, STRING_LIT("42")));
  arena_free(a);
}

Test(string, sb_append_int_negative) {
  Arena *a = arena_create(256);
  StringBuilder sb = sb_create(arena_allocator(a));
  sb_append_int(&sb, -123);
  String result = sb_finish(&sb);
  cr_assert(string_equals(result, STRING_LIT("-123")));
  arena_free(a);
}

Test(string, sb_append_int_zero) {
  Arena *a = arena_create(256);
  StringBuilder sb = sb_create(arena_allocator(a));
  sb_append_int(&sb, 0);
  String result = sb_finish(&sb);
  cr_assert(string_equals(result, STRING_LIT("0")));
  arena_free(a);
}

Test(string, sb_append_fmt_basic) {
  Arena *a = arena_create(512);
  StringBuilder sb = sb_create(arena_allocator(a));
  sb_append_fmt(&sb, "hello %s, you are %d years old", "world", 30);
  String result = sb_finish(&sb);
  cr_assert(
      string_equals(result, STRING_LIT("hello world, you are 30 years old")));
  arena_free(a);
}

Test(string, sb_append_fmt_compose) {
  Arena *a = arena_create(512);
  StringBuilder sb = sb_create(arena_allocator(a));
  sb_append_cstr(&sb, "x=");
  sb_append_fmt(&sb, "%d", 7);
  sb_append_cstr(&sb, ", y=");
  sb_append_fmt(&sb, "%.2f", 3.14);
  String result = sb_finish(&sb);
  cr_assert(string_equals(result, STRING_LIT("x=7, y=3.14")));
  arena_free(a);
}

/* --- string_replace ----------------------------------------------------- */

Test(string, replace_basic) {
  Arena *a = arena_create(512);
  String r =
      string_replace(STRING_LIT("hello world world"), STRING_LIT("world"),
                     STRING_LIT("there"), arena_allocator(a));
  cr_assert(string_equals(r, STRING_LIT("hello there there")));
  arena_free(a);
}

Test(string, replace_no_match) {
  Arena *a = arena_create(256);
  String r = string_replace(STRING_LIT("hello"), STRING_LIT("xyz"),
                            STRING_LIT("!"), arena_allocator(a));
  cr_assert(string_equals(r, STRING_LIT("hello")));
  arena_free(a);
}

Test(string, replace_empty_needle_returns_copy) {
  Arena *a = arena_create(256);
  String r = string_replace(STRING_LIT("hello"), STRING_LIT(""),
                            STRING_LIT("X"), arena_allocator(a));
  cr_assert(string_equals(r, STRING_LIT("hello")));
  arena_free(a);
}

Test(string, replace_whole_string) {
  Arena *a = arena_create(256);
  String r = string_replace(STRING_LIT("aaa"), STRING_LIT("a"),
                            STRING_LIT("bb"), arena_allocator(a));
  cr_assert(string_equals(r, STRING_LIT("bbbbbb")));
  arena_free(a);
}

Test(string, replace_with_empty_replacement) {
  Arena *a = arena_create(256);
  String r = string_replace(STRING_LIT("a,b,c"), STRING_LIT(","),
                            STRING_LIT(""), arena_allocator(a));
  cr_assert(string_equals(r, STRING_LIT("abc")));
  arena_free(a);
}
