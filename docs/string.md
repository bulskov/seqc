# string

Bounded string type (`ptr + len`, not null-terminated), string builder, and
iterator sources for character-level and token-level iteration.

**Header:** `include/seqc/string.h`  
**See also:** [`iter`](iter.md) · [`hashmap`](hashmap.md) · [`arena`](arena.md)

---

## Type

### `string_t`

```c
typedef struct {
  const char *ptr;
  size_t      len;
} string_t;
```

A non-owning view. `ptr` need not be null-terminated.

### `STRING_LIT(s)`

```c
#define STRING_LIT(s) ((string_t){(s), sizeof(s) - 1})
```

Wrap a C string literal as a `string_t` at zero cost.

```c
string_t hello = STRING_LIT("hello");
```

### `STRING_NOT_FOUND`

```c
#define STRING_NOT_FOUND SIZE_MAX
```

Sentinel returned by `string_find` when no match is found.

### `STRING_FMT` / `STRING_ARG`

Macros for printing a `string_t` with the `printf` family without allocating —
see [Printing & stdio interop](#printing--stdio-interop).

---

## Construction

### `string_view_cstr`

```c
string_t string_view_cstr(const char *s);
```

Wrap a null-terminated C string as a non-owning `string_t` view. No copy is
made — the returned `string_t` must not outlive `s`. Prefer `STRING_LIT` for
string literals and `string_from_cstr` when the cstr lifetime is uncertain.

### `string_from_cstr`

```c
string_t string_from_cstr(const char *s, allocator_t allocator);
```

Copy `s` into allocator-owned memory and return an owning `string_t`. Safe to
use when you cannot guarantee that the original cstr will remain valid.

```c
Arena  *a = arena_create(256);
char    buf[] = "hello";
string_t  s = string_from_cstr(buf, arena_allocator(a));
buf[0] = 'X'; /* s.ptr[0] is still 'h' — independent copy */
```

### `string_copy`

```c
string_t string_copy(string_t s, allocator_t allocator);
```

Allocate an arena-owned copy of `s`.

### `string_to_cstr`

```c
const char *string_to_cstr(string_t s, allocator_t allocator);
```

Allocate an arena-owned null-terminated copy. Use when you need to pass a
`string_t` to a C API that expects `char *` and the copy must outlive the call.

### `string_to_cstr_buf`

```c
const char *string_to_cstr_buf(string_t s, char *buf, size_t bufsize);
```

Copy `s` into a caller-supplied buffer and null-terminate it — for passing to
APIs that require a `const char *` (e.g. `fopen`, `getenv`) without allocating.
Returns `buf`, or `NULL` if `buf` is `NULL` or too small to hold `s.len + 1`
bytes.

```c
char  path[256];
FILE *f = fopen(string_to_cstr_buf(p, path, sizeof path), "r");
```

---

## Printing & stdio interop

`string_t` is length-delimited and not null-terminated, so it cannot be passed
directly to `printf("%s", ...)`. These helpers bridge to standard I/O without
forcing an allocation.

### `STRING_FMT` / `STRING_ARG`

```c
#define STRING_FMT     "%.*s"
#define STRING_ARG(s)  (int)(s).len, (s).ptr
```

Print a `string_t` with the `printf` family using the precision form `"%.*s"`,
which reads exactly `len` bytes and needs no terminator. Zero allocation.

```c
printf("hello, " STRING_FMT "!\n", STRING_ARG(name));
fprintf(stderr, "bad token " STRING_FMT " at %zu\n", STRING_ARG(tok), pos);
```

Caveats: the precision argument is an `int`, so a `string_t` longer than
`INT_MAX` is truncated; `STRING_ARG(s)` evaluates `s` twice (avoid arguments
with side effects); and `"%.*s"` stops at an embedded NUL. For binary-safe
output, use the `string_io.h` helpers below.

### `string_io.h` write helpers

**Header:** `include/seqc/string_io.h` — kept separate from `string.h` so the
core string type carries no `<stdio.h>` dependency.

```c
size_t string_fwrite(string_t s, FILE *f);  /* write s.len bytes to f   */
size_t string_print(string_t s);            /* write s to stdout        */
size_t string_println(string_t s);          /* write s + '\n' to stdout */
```

Each writes exactly `s.len` bytes and is binary-safe — embedded NULs are
preserved. Each returns the number of bytes written; `string_fwrite` returns
`0` for a `NULL` stream or an empty string, and `string_println` counts the
trailing newline.

```c
#include "seqc/string_io.h"

string_println(STRING_LIT("hello"));      /* "hello\n" to stdout       */
size_t n = string_fwrite(body, logfile);  /* exact bytes, NUL-safe     */
```

---

## Comparison

### `string_equals`

```c
bool string_equals(string_t a, string_t b);
```

Return `true` if `a` and `b` have the same length and byte content.

### `string_compare`

```c
int string_compare(string_t a, string_t b);
```

Lexicographic comparison. Returns negative, zero, or positive (like `strcmp`).
Note: signature is `(string_t, string_t)`, not `(const void *, const void *)` — it
cannot be passed directly as a [`compare_fn`](iter.md#function-pointer-types).
Wrap it:

```c
static int string_cmp(const void *a, const void *b) {
    return string_compare(*(const string_t *)a, *(const string_t *)b);
}
```

---

## Query

### `string_starts_with` / `string_ends_with`

```c
bool string_starts_with(string_t s, string_t prefix);
bool string_ends_with(string_t s, string_t suffix);
```

### `string_contains`

```c
bool string_contains(string_t s, string_t needle);
```

### `string_find`

```c
size_t string_find(string_t s, string_t needle);
```

Return the byte offset of the first occurrence of `needle`, or
`STRING_NOT_FOUND`.

```c
size_t pos = string_find(STRING_LIT("hello world"), STRING_LIT("world"));
// pos == 6
```

---

## Transformation

### `string_replace`

```c
string_t string_replace(string_t s, string_t needle, string_t replacement,
                      allocator_t allocator);
```

Return a new arena-allocated `string_t` with all non-overlapping occurrences of
`needle` replaced by `replacement`. Replacements proceed left-to-right. If
`needle` is empty the original string is returned as a copy unchanged.

```c
Arena  *a   = arena_create(4096);
string_t  res = string_replace(STRING_LIT("a,b,c"), STRING_LIT(","),
                             STRING_LIT(" | "), arena_allocator(a));
// res == "a | b | c"
arena_free(a);
```

### `string_to_uppercase` / `string_to_lowercase`

```c
string_t string_to_uppercase(string_t s, allocator_t allocator);
string_t string_to_lowercase(string_t s, allocator_t allocator);
```

Return a new arena-allocated `string_t` with every ASCII letter converted to
upper or lower case. Non-letter bytes are copied unchanged.

```c
Arena  *a = arena_create(256);
string_t  u = string_to_uppercase(STRING_LIT("Hello World!"), arena_allocator(a));
// u == "HELLO WORLD!"
arena_free(a);
```

### `string_join`

```c
string_t string_join(iter_t it, string_t sep, allocator_t allocator);
```

Consume `it` (an iterator that yields `string_t` values), concatenate every
token with `sep` inserted between consecutive tokens, and return the result as
a new arena-allocated `string_t`. The iterator is dropped after the call.

```c
Arena  *a   = arena_create(512);
iter_t    it  = string_split_substr(STRING_LIT("a,b,c"), STRING_LIT(","),
                                  arena_allocator(a));
string_t  res = string_join(it, STRING_LIT(" | "), arena_allocator(a));
// res == "a | b | c"
arena_free(a);
```

### `string_to_int`

```c
bool string_to_int(string_t s, long long *out);
```

Parse `s` as a base-10 integer. On success writes the result to `*out` and
returns `true`. Returns `false` if `s` is empty, contains non-numeric
characters, has trailing garbage, or the value overflows `long long`. No
allocation is performed.

```c
long long val;
if (string_to_int(STRING_LIT("-42"), &val))
    printf("%lld\n", val);  // -42
```

### `string_to_double`

```c
bool string_to_double(string_t s, double *out);
```

Parse `s` as a floating-point number (same syntax accepted by `strtod`). On
success writes the result to `*out` and returns `true`. Returns `false` if `s`
is empty, contains non-numeric characters, has trailing garbage, or the value
overflows to infinity. No allocation is performed.

```c
double val;
if (string_to_double(STRING_LIT("3.14"), &val))
    printf("%f\n", val);  // 3.140000
```

---

## Views (zero-copy)

### `string_slice`

```c
string_t string_slice(string_t s, size_t start, size_t end);
```

Return a sub-string view `s[start..end)`. No allocation.

### `string_trim` / `string_trim_left` / `string_trim_right`

```c
string_t string_trim(string_t s);
string_t string_trim_left(string_t s);
string_t string_trim_right(string_t s);
```

Return views with leading / trailing / both ASCII whitespace removed. No
allocation.

```c
string_t t = string_trim(STRING_LIT("  hello  "));
// t.ptr points into the original buffer
```

---

## strbuf_t

Builds a `string_t` incrementally using a [`vec_t`](vec.md) of `char`.

### `strbuf_create`

```c
strbuf_t *strbuf_create(allocator_t allocator);
```

### `strbuf_append`

```c
seqc_status_t strbuf_append(strbuf_t *sb, string_t s);
```

Returns `SEQC_OOM` if the underlying vec_t fails to grow; `SEQC_OK` otherwise.

### `strbuf_append_char`

```c
seqc_status_t strbuf_append_char(strbuf_t *sb, char c);
```

### `strbuf_append_cstr`

```c
seqc_status_t strbuf_append_cstr(strbuf_t *sb, const char *s);
```

### `strbuf_append_int`

```c
seqc_status_t strbuf_append_int(strbuf_t *sb, long long value);
```

Format `value` with `snprintf` and append. Returns `SEQC_OOM` on failure.

### `strbuf_append_fmt`

```c
seqc_status_t strbuf_append_fmt(strbuf_t *sb, const char *fmt, ...);
```

`printf`-style formatting. Uses a 256-byte stack buffer for short results;
falls back to an arena allocation for longer output. Returns `SEQC_OOM` on
allocation failure.

### `strbuf_finish`

```c
string_t strbuf_finish(const strbuf_t *sb);
```

Return a `string_t` view over the builder's buffer. The view is valid as long as
the arena that backs the builder is alive. No copy is made.

```c
Arena         *a  = arena_create(4096);
strbuf_t *sb = strbuf_create(arena_allocator(a));

strbuf_append_cstr(sb, "x=");
strbuf_append_int(sb, 42);
strbuf_append_fmt(sb, ", pi=%.4f", 3.14159);

string_t result = strbuf_finish(sb);
// result == "x=42, pi=3.1416"

arena_free(a);
```

---

## HashMap / Set helpers

```c
size_t string_hash(const void *key, size_t key_size);
int    string_key_eq(const void *a, const void *b, size_t key_size);
```

Pass these as `hash_fn` / `eq_fn` when the key type of a
[`hashmap_t`](hashmap.md) or [`set_t`](set.md) is `string_t`.

```c
hashmap_t *m = hashmap_create(sizeof(string_t), sizeof(int),
                             string_hash, string_key_eq,
                             arena_allocator(a));
string_t k = STRING_LIT("count");
int    v = 7;
hashmap_set(m, &k, &v);
```

---

## Iter sources

### `string_chars`

```c
iter_t string_chars(string_t s, allocator_t allocator);
```

Yield each `char` in order.

### `string_chars_rev`

```c
iter_t string_chars_rev(string_t s, allocator_t allocator);
```

Yield each `char` in reverse order.

### `string_split_substr`

```c
iter_t string_split_substr(string_t s, string_t delim, allocator_t allocator);
```

Yield `string_t` tokens separated by every non-overlapping occurrence of the
substring `delim`. Empty tokens are kept (adjacent or trailing delimiters
produce empty tokens), so the split round-trips:
`string_join(string_split_substr(s, d, a), d, a) == s`. An empty `delim`
matches nowhere and yields `s` as a single token — use `string_chars` for
per-character iteration.

```c
iter_t    it  = string_split_substr(STRING_LIT("a,b,c"), STRING_LIT(","),
                                  arena_allocator(a));
string_t  tok;
while (it.next(&it, &tok))
    printf(STRING_FMT "\n", STRING_ARG(tok));
iter_drop(&it);
// prints: a / b / c
```

### `string_split_any`

```c
iter_t string_split_any(string_t s, string_t set, allocator_t allocator);
```

Split `s` on **any** single character contained in `set`; each matching
character is its own boundary. Like `string_split_substr`, empty tokens are
kept — so leading/trailing separators and runs of delimiter characters yield
empty tokens. For whitespace-style tokenisation, compose with `iter_filter` to
drop the empties:

```c
// keep-empty split on any whitespace char
iter_t    raw = string_split_any(STRING_LIT("  the\tquick \n"),
                               STRING_LIT(" \t\n"), arena_allocator(a));
// skip-empty tokenisation = split + filter
iter_t    it  = iter_filter(raw, non_empty_str, NULL);
string_t  tok;
while (it.next(&it, &tok))
    printf(STRING_FMT "\n", STRING_ARG(tok));
iter_drop(&it);
// prints: the / quick
```

An empty `set` matches nowhere and yields `s` as a single token.
