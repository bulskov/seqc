#pragma once

#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h" /* for Allocator, Scratch */
#include "iter/iter.h"
#include "vec/vec.h"

typedef struct {
  const char *ptr;
  size_t len;
} String;

/* Wrap a string literal without copying */
#define STRING_LIT(s) ((String){(s), sizeof(s) - 1})

/* SIZE_MAX sentinel returned by string_find when not found */
#define STRING_NOT_FOUND SIZE_MAX

/* --- Construction ------------------------------------------------------- */

String string_from_cstr(const char *s);            /* wraps, no copy      */
String string_copy(String s, Allocator allocator); /* allocator copy      */
const char *string_to_cstr(String s,
                           Allocator allocator); /* copy + null-terminate */

/* --- Comparison --------------------------------------------------------- */

int string_equals(String a, String b);
int string_compare(String a, String b); /* <0, 0, >0           */

/* --- Query -------------------------------------------------------------- */

int string_starts_with(String s, String prefix);
int string_ends_with(String s, String suffix);
int string_contains(String s, String needle);
size_t string_find(String s, String needle); /* STRING_NOT_FOUND if absent */

/* --- Views (zero-copy) -------------------------------------------------- */

String string_slice(String s, size_t start, size_t end);
String string_trim(String s);
String string_trim_left(String s);
String string_trim_right(String s);

/* --- Builder ------------------------------------------------------------ */

typedef struct {
  Vec chars;
} StringBuilder;

StringBuilder sb_create(Allocator allocator);
void sb_append(StringBuilder *sb, String s);
void sb_append_char(StringBuilder *sb, char c);
void sb_append_cstr(StringBuilder *sb, const char *s);
String sb_finish(const StringBuilder *sb); /* view — no copy      */

/* --- HashMap helpers ---------------------------------------------------- */

/* Use as hash_fn when the hashmap key type is String */
size_t string_hash(const void *key, size_t key_size);

/* Use as eq_fn when the hashmap key type is String */
int string_key_eq(const void *a, const void *b, size_t key_size);

/* --- Iter sources ------------------------------------------------------- */

/* Yields char, one per character */
Iter string_chars(String s, Allocator allocator);
/* Yields char in reverse order */
Iter string_chars_rev(String s, Allocator allocator);

/* Yields String tokens split by delim */
Iter string_split(String s, String delim, Allocator allocator);
