# string

Bounded string type (`ptr + len`, not null-terminated), string builder, and
iterator sources for character-level and token-level iteration.

**Header:** `src/string/string.h`  
**See also:** [`iter`](iter.md) · [`hashmap`](hashmap.md) · [`arena`](arena.md)

---

## Type

### `String`

```c
typedef struct {
  const char *ptr;
  size_t      len;
} String;
```

A non-owning view. `ptr` need not be null-terminated.

### `STRING_LIT(s)`

```c
#define STRING_LIT(s) ((String){(s), sizeof(s) - 1})
```

Wrap a C string literal as a `String` at zero cost.

```c
String hello = STRING_LIT("hello");
```

### `STRING_NOT_FOUND`

```c
#define STRING_NOT_FOUND SIZE_MAX
```

Sentinel returned by `string_find` when no match is found.

---

## Construction

### `string_from_cstr`

```c
String string_from_cstr(const char *s);
```

Wrap a null-terminated C string as a `String`. No copy — the string must
outlive the returned view.

### `string_copy`

```c
String string_copy(String s, Allocator allocator);
```

Allocate an arena-owned copy of `s`.

### `string_to_cstr`

```c
const char *string_to_cstr(String s, Allocator allocator);
```

Allocate an arena-owned null-terminated copy. Use when you need to pass a
`String` to a C API that expects `char *`.

---

## Comparison

### `string_equals`

```c
int string_equals(String a, String b);
```

Return `1` if `a` and `b` have the same length and byte content.

### `string_compare`

```c
int string_compare(String a, String b);
```

Lexicographic comparison. Returns negative, zero, or positive (like `strcmp`).
Note: signature is `(String, String)`, not `(const void *, const void *)` — it
cannot be passed directly as a [`compare_fn`](iter.md#function-pointer-types).
Wrap it:

```c
static int string_cmp(const void *a, const void *b) {
    return string_compare(*(const String *)a, *(const String *)b);
}
```

---

## Query

### `string_starts_with` / `string_ends_with`

```c
int string_starts_with(String s, String prefix);
int string_ends_with(String s, String suffix);
```

### `string_contains`

```c
int string_contains(String s, String needle);
```

### `string_find`

```c
size_t string_find(String s, String needle);
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
String string_replace(String s, String needle, String replacement,
                      Allocator allocator);
```

Return a new arena-allocated `String` with all non-overlapping occurrences of
`needle` replaced by `replacement`. Replacements proceed left-to-right. If
`needle` is empty the original string is returned as a copy unchanged.

```c
Arena  *a   = arena_create(4096);
String  res = string_replace(STRING_LIT("a,b,c"), STRING_LIT(","),
                             STRING_LIT(" | "), arena_allocator(a));
// res == "a | b | c"
arena_free(a);
```

---

## Views (zero-copy)

### `string_slice`

```c
String string_slice(String s, size_t start, size_t end);
```

Return a sub-string view `s[start..end)`. No allocation.

### `string_trim` / `string_trim_left` / `string_trim_right`

```c
String string_trim(String s);
String string_trim_left(String s);
String string_trim_right(String s);
```

Return views with leading / trailing / both ASCII whitespace removed. No
allocation.

```c
String t = string_trim(STRING_LIT("  hello  "));
// t.ptr points into the original buffer
```

---

## StringBuilder

Builds a `String` incrementally using a [`Vec`](vec.md) of `char`.

### `sb_create`

```c
StringBuilder sb_create(Allocator allocator);
```

### `sb_append`

```c
void sb_append(StringBuilder *sb, String s);
```

### `sb_append_char`

```c
void sb_append_char(StringBuilder *sb, char c);
```

### `sb_append_cstr`

```c
void sb_append_cstr(StringBuilder *sb, const char *s);
```

### `sb_append_int`

```c
void sb_append_int(StringBuilder *sb, long long value);
```

Format `value` with `snprintf` and append.

### `sb_append_fmt`

```c
void sb_append_fmt(StringBuilder *sb, const char *fmt, ...);
```

`printf`-style formatting. Uses a 256-byte stack buffer for short results;
falls back to an arena allocation for longer output.

### `sb_finish`

```c
String sb_finish(const StringBuilder *sb);
```

Return a `String` view over the builder's buffer. The view is valid as long as
the arena that backs the builder is alive. No copy is made.

```c
Arena         *a  = arena_create(4096);
StringBuilder  sb = sb_create(arena_allocator(a));

sb_append_cstr(&sb, "x=");
sb_append_int(&sb, 42);
sb_append_fmt(&sb, ", pi=%.4f", 3.14159);

String result = sb_finish(&sb);
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
[`HashMap`](hashmap.md) or [`Set`](set.md) is `String`.

```c
HashMap m = hashmap_create(sizeof(String), sizeof(int),
                            string_hash, string_key_eq,
                            arena_allocator(a));
String k = STRING_LIT("count");
int    v = 7;
hashmap_set(&m, &k, &v);
```

---

## Iter sources

### `string_chars`

```c
Iter string_chars(String s, Allocator allocator);
```

Yield each `char` in order.

### `string_chars_rev`

```c
Iter string_chars_rev(String s, Allocator allocator);
```

Yield each `char` in reverse order.

### `string_split`

```c
Iter string_split(String s, String delim, Allocator allocator);
```

Yield `String` tokens separated by `delim`. Adjacent delimiters produce empty
tokens.

```c
Iter    it  = string_split(STRING_LIT("a,b,c"), STRING_LIT(","),
                           arena_allocator(a));
String  tok;
while (it.next(&it, &tok))
    printf("%.*s\n", (int)tok.len, tok.ptr);
iter_drop(&it);
// prints: a / b / c
```
