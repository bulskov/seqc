#pragma once

#include <stddef.h>
#include <stdint.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

#include <stdarg.h>

typedef struct
{
    const char *ptr;
    size_t len;
} string_t;

/* Wrap a string literal without copying */
#define STRING_LIT(s) ((string_t){(s), sizeof(s) - 1})

/* printf-family interop: print a string without allocating or
 * NUL-terminating, using the precision form "%.*s".  For example:
 *
 *     printf("hello, " STRING_FMT "!\n", STRING_ARG(name));
 *
 * Caveats: precision is an int, so a string longer than INT_MAX is
 * truncated; STRING_ARG evaluates its argument twice (avoid side effects);
 * and "%.*s" stops at an embedded NUL.  For binary-safe, NUL-tolerant
 * output, use the write helpers in seqc/string_io.h instead. */
#define STRING_FMT "%.*s"
#define STRING_ARG(s) (int)(s).len, (s).ptr

/* SIZE_MAX sentinel returned by string_find when not found */
#define STRING_NOT_FOUND SIZE_MAX

/* --- Construction ------------------------------------------------------- */

/* Wrap a null-terminated C string without copying.  The caller retains
 * ownership; the returned string_t must not outlive the original cstr. */
string_t string_view_cstr(const char *s);

/* Copy s into allocator-owned memory, returning an owning string_t. */
string_t string_from_cstr(const char *s, allocator_t allocator);

string_t string_copy(string_t s, allocator_t allocator); /* allocator copy */
const char *string_to_cstr(
    string_t s, allocator_t allocator); /* copy + null-terminate */

/* Copy s into a caller-provided buffer and null-terminate it, for passing to
 * APIs that require a const char * (fopen, getenv, ...) without allocating.
 * Returns buf, or NULL if buf is NULL or too small to hold s.len + 1 bytes. */
const char *string_to_cstr_buf(string_t s, char *buf, size_t bufsize);

/* --- Comparison --------------------------------------------------------- */

bool string_equals(string_t a, string_t b);
bool string_equals_case_insensitive(string_t a, string_t b);
int string_compare(string_t a, string_t b); /* <0, 0, >0           */

/* --- Query -------------------------------------------------------------- */

bool string_starts_with(string_t s, string_t prefix);
bool string_ends_with(string_t s, string_t suffix);
bool string_contains(string_t s, string_t needle);
size_t string_find(
    string_t s, string_t needle); /* STRING_NOT_FOUND if absent */

/* --- Transformation ----------------------------------------------------- */

/* Return a new string_t with all non-overlapping occurrences of needle replaced
 * by replacement.  The result is arena-allocated via allocator. */
string_t string_replace(
    string_t s, string_t needle, string_t replacement, allocator_t allocator);

/* Return a new arena-allocated copy of s with all ASCII letters uppercased. */
string_t string_to_uppercase(string_t s, allocator_t allocator);

/* Return a new arena-allocated copy of s with all ASCII letters lowercased. */
string_t string_to_lowercase(string_t s, allocator_t allocator);

/* Join all string_t values yielded by it, separated by sep.
 * Consumes and drops the iterator.  Result is arena-allocated via allocator. */
string_t string_join(iter_t it, string_t sep, allocator_t allocator);

/* Parse a base-10 integer from s.  Writes to *out and returns true on success.
 * Returns false if s is empty, contains non-numeric characters, or the value
 * is outside the range of long long. */
bool string_to_int(string_t s, long long *out);

/* Parse a double from s.  Writes to *out and returns true on success.
 * Returns false if s is empty, contains non-numeric characters, or the value
 * overflows to infinity. */
bool string_to_double(string_t s, double *out);

/* --- Views (zero-copy) -------------------------------------------------- */

string_t string_slice(string_t s, size_t start, size_t end);
string_t string_trim(string_t s);
string_t string_trim_left(string_t s);
string_t string_trim_right(string_t s);

/* --- Builder ------------------------------------------------------------ */

typedef struct strbuf_t strbuf_t;

strbuf_t *strbuf_create(allocator_t allocator);
seqc_status_t strbuf_append(strbuf_t *sb, string_t s);
seqc_status_t strbuf_append_char(strbuf_t *sb, char c);
seqc_status_t strbuf_append_cstr(strbuf_t *sb, const char *s);
seqc_status_t strbuf_append_int(strbuf_t *sb, long long value);
seqc_status_t strbuf_append_fmt(strbuf_t *sb, const char *fmt, ...);
string_t strbuf_finish(const strbuf_t *sb); /* view — no copy      */
size_t strbuf_len(const strbuf_t *sb);

/* --- hashmap_t helpers ---------------------------------------------------- */

/* Use as hash_fn when the hashmap key type is string_t */
size_t string_hash(const void *key, size_t key_size);

/* Use as eq_fn when the hashmap key type is string_t */
bool string_key_eq(const void *a, const void *b, size_t key_size);

/* --- iter_t sources ------------------------------------------------------- */

/* Yields char, one per character */
iter_t string_chars(string_t s, allocator_t allocator);
/* Yields char in reverse order */
iter_t string_chars_rev(string_t s, allocator_t allocator);

/* Split s on every non-overlapping occurrence of the substring delim and
 * yield the string tokens between them.  Empty tokens are kept, so the result
 * round-trips: string_join(string_split_substr(s, d, a), d, a) == s.
 * An empty delim matches nowhere and yields s as a single token (use
 * string_chars for per-character iteration). */
iter_t string_split_substr(string_t s, string_t delim, allocator_t allocator);

/* Split s on any single character contained in the set `set`; each matching
 * character is its own boundary.  Empty tokens are kept (e.g. adjacent or
 * leading/trailing separators yield empty tokens) — filter them with
 * iter_filter for whitespace-style tokenisation.  An empty set yields s as a
 * single token. */
iter_t string_split_any(string_t s, string_t set, allocator_t allocator);
