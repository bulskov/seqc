#pragma once

#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h" /* for Allocator, Scratch */
#include "iter/iter.h"


#include <stdarg.h>

typedef struct
{
    const char *ptr;
    size_t len;
} String;

/* Wrap a string literal without copying */
#define STRING_LIT(s) ((String){(s), sizeof(s) - 1})

/* SIZE_MAX sentinel returned by string_find when not found */
#define STRING_NOT_FOUND SIZE_MAX

/* --- Construction ------------------------------------------------------- */

/* Wrap a null-terminated C string without copying.  The caller retains
 * ownership; the returned String must not outlive the original cstr. */
String string_view_cstr(const char *s);

/* Copy s into allocator-owned memory, returning an owning String. */
String string_from_cstr(const char *s, Allocator allocator);

String string_copy(String s, Allocator allocator); /* allocator copy      */
const char *string_to_cstr(
    String s, Allocator allocator); /* copy + null-terminate */

/* --- Comparison --------------------------------------------------------- */

bool string_equals(String a, String b);
int string_compare(String a, String b); /* <0, 0, >0           */

/* --- Query -------------------------------------------------------------- */

bool string_starts_with(String s, String prefix);
bool string_ends_with(String s, String suffix);
bool string_contains(String s, String needle);
size_t string_find(String s, String needle); /* STRING_NOT_FOUND if absent */

/* --- Transformation ----------------------------------------------------- */

/* Return a new String with all non-overlapping occurrences of needle replaced
 * by replacement.  The result is arena-allocated via allocator. */
String string_replace(
    String s, String needle, String replacement, Allocator allocator);

/* Return a new arena-allocated copy of s with all ASCII letters uppercased. */
String string_to_uppercase(String s, Allocator allocator);

/* Return a new arena-allocated copy of s with all ASCII letters lowercased. */
String string_to_lowercase(String s, Allocator allocator);

/* Join all String values yielded by it, separated by sep.
 * Consumes and drops the iterator.  Result is arena-allocated via allocator. */
String string_join(Iter it, String sep, Allocator allocator);

/* Parse a base-10 integer from s.  Writes to *out and returns true on success.
 * Returns false if s is empty, contains non-numeric characters, or the value
 * is outside the range of long long. */
bool string_to_int(String s, long long *out);

/* Parse a double from s.  Writes to *out and returns true on success.
 * Returns false if s is empty, contains non-numeric characters, or the value
 * overflows to infinity. */
bool string_to_double(String s, double *out);

/* --- Views (zero-copy) -------------------------------------------------- */

String string_slice(String s, size_t start, size_t end);
String string_trim(String s);
String string_trim_left(String s);
String string_trim_right(String s);

/* --- Builder ------------------------------------------------------------ */

typedef struct StringBuilder StringBuilder;

StringBuilder *sb_create(Allocator allocator);
SeqcStatus sb_append(StringBuilder *sb, String s);
SeqcStatus sb_append_char(StringBuilder *sb, char c);
SeqcStatus sb_append_cstr(StringBuilder *sb, const char *s);
SeqcStatus sb_append_int(StringBuilder *sb, long long value);
SeqcStatus sb_append_fmt(StringBuilder *sb, const char *fmt, ...);
String sb_finish(const StringBuilder *sb); /* view — no copy      */

/* --- HashMap helpers ---------------------------------------------------- */

/* Use as hash_fn when the hashmap key type is String */
size_t string_hash(const void *key, size_t key_size);

/* Use as eq_fn when the hashmap key type is String */
bool string_key_eq(const void *a, const void *b, size_t key_size);

/* --- Iter sources ------------------------------------------------------- */

/* Yields char, one per character */
Iter string_chars(String s, Allocator allocator);
/* Yields char in reverse order */
Iter string_chars_rev(String s, Allocator allocator);

/* Yields String tokens split by delim */
Iter string_split(String s, String delim, Allocator allocator);
