#pragma once

#include <stddef.h>
#include <stdint.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

#include <stdarg.h>

/* Marks a function that is kept for compatibility and will be removed in
 * the next major version: using it gives a compiler warning. */
#if defined(__GNUC__) || defined(__clang__)
#define SEQC_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#define SEQC_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define SEQC_DEPRECATED(msg)
#endif

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

/* Like string_compare, but ASCII letters are folded to lower case first, so
 * "B" sorts after "_" (as strcasecmp does).  Other bytes, including UTF-8,
 * compare unchanged.  Returns 0 exactly when string_equals_case_insensitive
 * is true. */
int string_compare_case_insensitive(string_t a, string_t b);

/* --- Query -------------------------------------------------------------- */

bool string_starts_with(string_t s, string_t prefix);
bool string_ends_with(string_t s, string_t suffix);
bool string_contains(string_t s, string_t needle);
size_t string_find(
    string_t s, string_t needle); /* STRING_NOT_FOUND if absent */

/* Position of the first / last byte equal to c, or STRING_NOT_FOUND.
 * Searches exactly s.len bytes: embedded NULs are ordinary bytes, and
 * nothing past s.len is read. */
size_t string_find_char(string_t s, char c);
size_t string_rfind_char(string_t s, char c);

/* --- Transformation ----------------------------------------------------- */

/* Return a new string_t with all non-overlapping occurrences of needle replaced
 * by replacement.  The result is a plain allocation of exactly len bytes from
 * allocator: free it with mem_free(allocator, (void *)r.ptr, r.len).  An empty
 * result, or running out of memory, gives {NULL, 0}. */
string_t string_replace(
    string_t s, string_t needle, string_t replacement, allocator_t allocator);

/* Return a new copy of s (exactly s.len bytes from allocator) with all ASCII
 * letters uppercased. */
string_t string_to_uppercase(string_t s, allocator_t allocator);

/* Return a new copy of s (exactly s.len bytes from allocator) with all ASCII
 * letters lowercased. */
string_t string_to_lowercase(string_t s, allocator_t allocator);

/* Join all string_t values yielded by it, separated by sep.  Consumes and
 * destroys the iterator.  The result is a plain allocation of exactly len
 * bytes from allocator, like string_replace. */
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

/* Allocation-free tokenizer: splits *rest at the first byte equal to sep.
 * Writes the piece before it to *token (a view), advances *rest past the
 * separator, and returns true.  Empty pieces are kept, as in
 * string_split_substr: "a//b/" gives "a", "", "b", "".  After the last
 * piece *rest becomes {NULL, 0} and the next call returns false; "" gives
 * one empty piece, {NULL, 0} gives none.
 *
 *     string_t rest = path, part;
 *     while (string_split_next(&rest, '/', &part))
 *         ...
 */
bool string_split_next(string_t *rest, char sep, string_t *token);

/* --- Builder ------------------------------------------------------------ */

typedef struct strbuf_t strbuf_t;

strbuf_t *strbuf_create(allocator_t allocator);
seqc_status_t strbuf_append(strbuf_t *sb, string_t s);
seqc_status_t strbuf_append_char(strbuf_t *sb, char c);
seqc_status_t strbuf_append_cstr(strbuf_t *sb, const char *s);
seqc_status_t strbuf_append_int(strbuf_t *sb, long long value);
seqc_status_t strbuf_append_fmt(strbuf_t *sb, const char *fmt, ...);
size_t strbuf_len(const strbuf_t *sb);
bool strbuf_is_empty(const strbuf_t *sb);

/* A copy of what has been built: a plain allocation of exactly len bytes
 * from allocator, yours to keep after strbuf_clear and strbuf_destroy.  Free
 * it with mem_free(allocator, (void *)s.ptr, s.len).  An empty builder, a
 * NULL builder or running out of memory gives {NULL, 0}.
 *
 *     string_t out = strbuf_to_string(sb, alloc);
 *     strbuf_destroy(sb);
 */
string_t strbuf_to_string(const strbuf_t *sb, allocator_t allocator);

/* A view of what has been built — no copy.  Valid only until the next
 * append, strbuf_clear or strbuf_destroy (an append may move the buffer).
 * Use it to look at or print the result; use strbuf_to_string to keep it. */
string_t strbuf_view(const strbuf_t *sb);

/* Deprecated since 3.1: the old name of strbuf_view.  Removed in 4.0. */
SEQC_DEPRECATED("use strbuf_view, or strbuf_to_string for a copy")
string_t strbuf_finish(const strbuf_t *sb);

/* Empty the builder for reuse.  Keeps its memory, so building again does
 * not allocate until the old capacity is exceeded.  Views returned by
 * strbuf_view before the clear see the bytes that are appended next. */
void strbuf_clear(strbuf_t *sb);

/* Shorten the builder to its first len bytes, keeping its memory — undo
 * appends back to a length saved with strbuf_len.  A len at or past
 * strbuf_len(sb) changes nothing; NULL is a no-op.
 *
 *     size_t mark = strbuf_len(sb);
 *     if (strbuf_append(sb, a) != SEQC_OK || strbuf_append(sb, b) != SEQC_OK)
 *         strbuf_truncate(sb, mark);   // all or nothing
 */
void strbuf_truncate(strbuf_t *sb, size_t len);

/* Release the builder and its buffer through its allocator.  Views
 * returned by strbuf_view become invalid — take a strbuf_to_string first
 * if you keep the result.  NULL is a no-op.  With an arena this frees
 * nothing that destroying the arena would not; with a malloc-style
 * allocator it is how the memory comes back. */
void strbuf_destroy(strbuf_t *sb);

/* --- hashmap_t helpers ---------------------------------------------------- */

/* Use as hash_fn when the hashmap key type is string_t */
size_t string_hash(const void *key, size_t key_size);

/* Use as eq_fn when the hashmap key type is string_t */
bool string_key_eq(const void *a, const void *b, size_t key_size);

/* The case-insensitive pair (ASCII letters folded, as
 * string_equals_case_insensitive): "README.md" and "readme.MD" are the same
 * key.  Use both together — equal keys must hash equal. */
size_t string_hash_case_insensitive(const void *key, size_t key_size);
bool string_key_eq_case_insensitive(
    const void *a, const void *b, size_t key_size);

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
