#include "string.h"
#include "vec/vec.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Construction ------------------------------------------------------- */

String string_from_cstr(const char *s)
{
    if (!s)
        return (String){NULL, 0};
    return (String){s, strlen(s)};
}

String string_copy(String s, Allocator allocator)
{
    if (!s.ptr || s.len == 0)
        return (String){NULL, 0};
    char *buf = allocator.alloc(allocator.ctx, s.len, 1);
    memcpy(buf, s.ptr, s.len);
    return (String){buf, s.len};
}

const char *string_to_cstr(String s, Allocator allocator)
{
    char *buf = allocator.alloc(allocator.ctx, s.len + 1, 1);
    if (s.ptr && s.len > 0)
        memcpy(buf, s.ptr, s.len);
    buf[s.len] = '\0';
    return buf;
}

/* --- Comparison --------------------------------------------------------- */

bool string_equals(String a, String b)
{
    return a.len == b.len && memcmp(a.ptr, b.ptr, a.len) == 0;
}

int string_compare(String a, String b)
{
    size_t min = a.len < b.len ? a.len : b.len;
    int cmp = memcmp(a.ptr, b.ptr, min);
    if (cmp != 0)
        return cmp;
    return (a.len > b.len) - (a.len < b.len);
}

/* --- Query -------------------------------------------------------------- */

bool string_starts_with(String s, String prefix)
{
    if (prefix.len > s.len)
        return false;
    return memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

bool string_ends_with(String s, String suffix)
{
    if (suffix.len > s.len)
        return false;
    return memcmp(s.ptr + s.len - suffix.len, suffix.ptr, suffix.len) == 0;
}

size_t string_find(String s, String needle)
{
    if (needle.len == 0)
        return 0;
    if (needle.len > s.len)
        return STRING_NOT_FOUND;
    for (size_t i = 0; i <= s.len - needle.len; i++)
    {
        if (memcmp(s.ptr + i, needle.ptr, needle.len) == 0)
            return i;
    }
    return STRING_NOT_FOUND;
}

bool string_contains(String s, String needle)
{
    return string_find(s, needle) != STRING_NOT_FOUND;
}

/* --- Views (zero-copy) -------------------------------------------------- */

String string_slice(String s, size_t start, size_t end)
{
    if (!s.ptr || start >= s.len || end <= start)
        return (String){NULL, 0};
    if (end > s.len)
        end = s.len;
    return (String){s.ptr + start, end - start};
}

String string_trim_left(String s)
{
    while (s.len > 0 && isspace((unsigned char)*s.ptr))
    {
        s.ptr++;
        s.len--;
    }
    return s;
}

String string_trim_right(String s)
{
    while (s.len > 0 && isspace((unsigned char)s.ptr[s.len - 1]))
        s.len--;
    return s;
}

String string_trim(String s)
{
    return string_trim_right(string_trim_left(s));
}

/* --- Transformation ----------------------------------------------------- */

String string_replace(
    String s, String needle, String replacement, Allocator allocator)
{
    StringBuilder *sb = sb_create(allocator);
    if (needle.len == 0)
    {
        /* Empty needle: return a copy unchanged */
        sb_append(sb, s);
        return sb_finish(sb);
    }
    size_t pos = 0;
    while (pos < s.len)
    {
        String remaining = string_slice(s, pos, s.len);
        size_t found = string_find(remaining, needle);
        if (found == STRING_NOT_FOUND)
        {
            sb_append(sb, remaining);
            break;
        }
        sb_append(sb, string_slice(remaining, 0, found));
        sb_append(sb, replacement);
        pos += found + needle.len;
    }
    return sb_finish(sb);
}

/* --- Transformation (case / join) --------------------------------------- */

String string_to_uppercase(String s, Allocator allocator)
{
    if (!s.ptr || s.len == 0)
        return (String){NULL, 0};
    char *buf = allocator.alloc(allocator.ctx, s.len, 1);
    for (size_t i = 0; i < s.len; i++)
        buf[i] = (char)toupper((unsigned char)s.ptr[i]);
    return (String){buf, s.len};
}

String string_to_lowercase(String s, Allocator allocator)
{
    if (!s.ptr || s.len == 0)
        return (String){NULL, 0};
    char *buf = allocator.alloc(allocator.ctx, s.len, 1);
    for (size_t i = 0; i < s.len; i++)
        buf[i] = (char)tolower((unsigned char)s.ptr[i]);
    return (String){buf, s.len};
}

/* --- Builder ------------------------------------------------------------ */

struct StringBuilder
{
    Vec *chars;
    Allocator allocator;
};

StringBuilder *sb_create(Allocator allocator)
{
    StringBuilder *sb = allocator.alloc(
        allocator.ctx, sizeof(StringBuilder), _Alignof(StringBuilder));
    if (!sb)
        return NULL;
    sb->chars = vec_create(sizeof(char), allocator);
    sb->allocator = allocator;
    return sb;
}

SeqcStatus sb_append(StringBuilder *sb, String s)
{
    for (size_t i = 0; i < s.len; i++)
    {
        SeqcStatus st = vec_push(sb->chars, &s.ptr[i]);
        if (st != SEQC_OK)
            return st;
    }
    return SEQC_OK;
}

SeqcStatus sb_append_char(StringBuilder *sb, char c)
{
    return vec_push(sb->chars, &c);
}

SeqcStatus sb_append_cstr(StringBuilder *sb, const char *s)
{
    return sb_append(sb, string_from_cstr(s));
}

String sb_finish(const StringBuilder *sb)
{
    Slice s = vec_as_slice(sb->chars);
    return (String){(const char *)s.ptr, s.len};
}

SeqcStatus sb_append_int(StringBuilder *sb, long long value)
{
    char buf[32];
    int n = snprintf(buf, sizeof buf, "%lld", value);
    if (n > 0)
        return sb_append_cstr(sb, buf);
    return SEQC_OK;
}

SeqcStatus sb_append_fmt(StringBuilder *sb, const char *fmt, ...)
{
    /* First pass: measure */
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n <= 0)
        return SEQC_OK;
    /* Second pass: write into a stack buffer (common case) or heap */
    char stack_buf[256];
    char *buf = (size_t)n + 1 <= sizeof stack_buf
                    ? stack_buf
                    : sb->allocator.alloc(
                          sb->allocator.ctx, (size_t)n + 1, _Alignof(char));
    if (!buf)
        return SEQC_OOM;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    SeqcStatus st = SEQC_OK;
    for (int i = 0; i < n; i++)
    {
        st = sb_append_char(sb, buf[i]);
        if (st != SEQC_OK)
            break;
    }
    if (buf != stack_buf && sb->allocator.free)
        sb->allocator.free(sb->allocator.ctx, buf);
    return st;
}

/* string_join is defined after the builder because it uses StringBuilder. */
String string_join(Iter it, String sep, Allocator allocator)
{
    StringBuilder *sb = sb_create(allocator);
    String token;
    bool first = true;
    while (it.next(&it, &token))
    {
        if (!first)
            sb_append(sb, sep);
        sb_append(sb, token);
        first = false;
    }
    iter_drop(&it);
    return sb_finish(sb);
}

/* --- string_to_int ------------------------------------------------------ */

bool string_to_int(String s, long long *out)
{
    if (!s.ptr || s.len == 0)
        return false;
    char buf[24]; /* enough for any long long: 19 digits + sign + NUL */
    if (s.len >= sizeof buf)
        return false;
    memcpy(buf, s.ptr, s.len);
    buf[s.len] = '\0';
    char *end;
    errno = 0;
    long long val = strtoll(buf, &end, 10);
    if (end == buf || *end != '\0' || errno == ERANGE)
        return false;
    *out = val;
    return true;
}

bool string_to_double(String s, double *out)
{
    if (!s.ptr || s.len == 0)
        return false;
    char buf[64];
    if (s.len >= sizeof buf)
        return false;
    memcpy(buf, s.ptr, s.len);
    buf[s.len] = '\0';
    char *end;
    errno = 0;
    double val = strtod(buf, &end);
    if (end == buf || *end != '\0' || errno == ERANGE)
        return false;
    *out = val;
    return true;
}

/* --- Iter sources ------------------------------------------------------- */

Iter string_chars(String s, Allocator allocator)
{
    Slice sl = {(void *)s.ptr, s.len, sizeof(char)};
    return iter_from_slice(sl, allocator);
}

Iter string_chars_rev(String s, Allocator allocator)
{
    Slice sl = {(void *)s.ptr, s.len, sizeof(char)};
    return iter_from_slice_rev(sl, allocator);
}

/* ---- string_split ------------------------------------------------------ */

typedef struct
{
    String src;
    String delim;
    size_t pos;
} SplitState;

static bool split_next(Iter *it, void *out)
{
    SplitState *s = it->state;
    if (s->pos > s->src.len)
        return false;

    String remaining = {s->src.ptr + s->pos, s->src.len - s->pos};
    size_t found = string_find(remaining, s->delim);

    String token;
    if (found == STRING_NOT_FOUND)
    {
        token = remaining;
        s->pos = s->src.len + 1; /* mark exhausted */
    }
    else
    {
        token = (String){remaining.ptr, found};
        s->pos += found + s->delim.len;
    }

    memcpy(out, &token, sizeof(String));
    return true;
}

static void split_drop(Iter *it)
{
    if (it->allocator.free)
        it->allocator.free(it->allocator.ctx, it->state);
}

Iter string_split(String s, String delim, Allocator allocator)
{
    SplitState *state = allocator.alloc(
        allocator.ctx, sizeof(SplitState), _Alignof(SplitState));
    *state = (SplitState){s, delim, 0};
    return (Iter){
        .next = split_next,
        .drop = split_drop,
        .state = state,
        .elem_size = sizeof(String),
        .allocator = allocator};
}

/* --- HashMap helpers ---------------------------------------------------- */

size_t string_hash(const void *key, size_t key_size)
{
    (void)key_size;
    if (!key)
        return 0;
    const String *s = (const String *)key;
    if (!s->ptr || s->len == 0)
        return 0;
    const uint8_t *data = (const uint8_t *)s->ptr;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < s->len; i++)
    {
        hash ^= (uint64_t)data[i];
        hash *= 1099511628211ULL;
    }
    return (size_t)hash;
}

bool string_key_eq(const void *a, const void *b, size_t key_size)
{
    (void)key_size;
    if (!a || !b)
        return false;
    return string_equals(*(const String *)a, *(const String *)b);
}
