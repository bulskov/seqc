#include "seqc/string.h"
#include "seqc/vec.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Construction ------------------------------------------------------- */

string_t string_view_cstr(const char *s)
{
    if (!s)
        return (string_t){NULL, 0};
    return (string_t){s, strlen(s)};
}

string_t string_from_cstr(const char *s, allocator_t allocator)
{
    if (!s)
        return (string_t){NULL, 0};
    size_t len = strlen(s);
    if (len == 0)
        return (string_t){NULL, 0};
    char *buf = mem_alloc(allocator, len, 1);
    if (!buf)
        return (string_t){NULL, 0};
    memcpy(buf, s, len);
    return (string_t){buf, len};
}

string_t string_copy(string_t s, allocator_t allocator)
{
    if (!s.ptr || s.len == 0)
        return (string_t){NULL, 0};
    char *buf = mem_alloc(allocator, s.len, 1);
    memcpy(buf, s.ptr, s.len);
    return (string_t){buf, s.len};
}

const char *string_to_cstr(string_t s, allocator_t allocator)
{
    char *buf = mem_alloc(allocator, s.len + 1, 1);
    if (!buf)
        return NULL;
    if (s.ptr && s.len > 0)
        memcpy(buf, s.ptr, s.len);
    buf[s.len] = '\0';
    return buf;
}

const char *string_to_cstr_buf(string_t s, char *buf, size_t bufsize)
{
    if (!buf || bufsize == 0)
        return NULL;
    if (s.len >= bufsize) /* need s.len bytes + a NUL; overflow-safe form */
        return NULL;
    if (s.ptr && s.len > 0)
        memcpy(buf, s.ptr, s.len);
    buf[s.len] = '\0';
    return buf;
}

/* --- Comparison --------------------------------------------------------- */

bool string_equals(string_t a, string_t b)
{
    return a.len == b.len && memcmp(a.ptr, b.ptr, a.len) == 0;
}

bool string_equals_case_insensitive(string_t a, string_t b)
{
    if (a.len != b.len)
        return false;
    for (size_t i = 0; i < a.len; i++)
    {
        if (tolower((unsigned char)a.ptr[i])
            != tolower((unsigned char)b.ptr[i]))
            return false;
    }
    return true;
}

int string_compare(string_t a, string_t b)
{
    size_t min = a.len < b.len ? a.len : b.len;
    int cmp = memcmp(a.ptr, b.ptr, min);
    if (cmp != 0)
        return cmp;
    return (a.len > b.len) - (a.len < b.len);
}

/* --- Query -------------------------------------------------------------- */

bool string_starts_with(string_t s, string_t prefix)
{
    if (prefix.len > s.len)
        return false;
    return memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

bool string_ends_with(string_t s, string_t suffix)
{
    if (suffix.len > s.len)
        return false;
    return memcmp(s.ptr + s.len - suffix.len, suffix.ptr, suffix.len) == 0;
}

size_t string_find(string_t s, string_t needle)
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

bool string_contains(string_t s, string_t needle)
{
    return string_find(s, needle) != STRING_NOT_FOUND;
}

/* --- Views (zero-copy) -------------------------------------------------- */

string_t string_slice(string_t s, size_t start, size_t end)
{
    if (!s.ptr || start >= s.len || end <= start)
        return (string_t){NULL, 0};
    if (end > s.len)
        end = s.len;
    return (string_t){s.ptr + start, end - start};
}

string_t string_trim_left(string_t s)
{
    while (s.len > 0 && isspace((unsigned char)*s.ptr))
    {
        s.ptr++;
        s.len--;
    }
    return s;
}

string_t string_trim_right(string_t s)
{
    while (s.len > 0 && isspace((unsigned char)s.ptr[s.len - 1]))
        s.len--;
    return s;
}

string_t string_trim(string_t s)
{
    return string_trim_right(string_trim_left(s));
}

/* --- Transformation ----------------------------------------------------- */

string_t string_replace(
    string_t s, string_t needle, string_t replacement, allocator_t allocator)
{
    strbuf_t *sb = strbuf_create(allocator);
    if (needle.len == 0)
    {
        /* Empty needle: return a copy unchanged */
        strbuf_append(sb, s);
        return strbuf_finish(sb);
    }
    size_t pos = 0;
    while (pos < s.len)
    {
        string_t remaining = string_slice(s, pos, s.len);
        size_t found = string_find(remaining, needle);
        if (found == STRING_NOT_FOUND)
        {
            strbuf_append(sb, remaining);
            break;
        }
        strbuf_append(sb, string_slice(remaining, 0, found));
        strbuf_append(sb, replacement);
        pos += found + needle.len;
    }
    return strbuf_finish(sb);
}

/* --- Transformation (case / join) --------------------------------------- */

string_t string_to_uppercase(string_t s, allocator_t allocator)
{
    if (!s.ptr || s.len == 0)
        return (string_t){NULL, 0};
    char *buf = mem_alloc(allocator, s.len, 1);
    for (size_t i = 0; i < s.len; i++)
        buf[i] = (char)toupper((unsigned char)s.ptr[i]);
    return (string_t){buf, s.len};
}

string_t string_to_lowercase(string_t s, allocator_t allocator)
{
    if (!s.ptr || s.len == 0)
        return (string_t){NULL, 0};
    char *buf = mem_alloc(allocator, s.len, 1);
    for (size_t i = 0; i < s.len; i++)
        buf[i] = (char)tolower((unsigned char)s.ptr[i]);
    return (string_t){buf, s.len};
}

/* --- Builder ------------------------------------------------------------ */

struct strbuf_t
{
    vec_t *chars;
    allocator_t allocator;
};

strbuf_t *strbuf_create(allocator_t allocator)
{
    strbuf_t *sb = mem_alloc(allocator, sizeof(strbuf_t), _Alignof(strbuf_t));
    if (!sb)
        return NULL;
    sb->chars = vec_create(sizeof(char), allocator);
    sb->allocator = allocator;
    return sb;
}

seqc_status_t strbuf_append(strbuf_t *sb, string_t s)
{
    for (size_t i = 0; i < s.len; i++)
    {
        seqc_status_t st = vec_push(sb->chars, &s.ptr[i]);
        if (st != SEQC_OK)
            return st;
    }
    return SEQC_OK;
}

seqc_status_t strbuf_append_char(strbuf_t *sb, char c)
{
    return vec_push(sb->chars, &c);
}

seqc_status_t strbuf_append_cstr(strbuf_t *sb, const char *s)
{
    return strbuf_append(sb, string_view_cstr(s));
}

string_t strbuf_finish(const strbuf_t *sb)
{
    slice_t s = vec_as_slice(sb->chars);
    return (string_t){(const char *)s.ptr, s.len};
}

size_t strbuf_len(const strbuf_t *sb)
{
    return vec_len(sb->chars);
}

seqc_status_t strbuf_append_int(strbuf_t *sb, long long value)
{
    char buf[32];
    int n = snprintf(buf, sizeof buf, "%lld", value);
    if (n > 0)
        return strbuf_append_cstr(sb, buf);
    return SEQC_OK;
}

seqc_status_t strbuf_append_fmt(strbuf_t *sb, const char *fmt, ...)
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
                    : mem_alloc(sb->allocator, (size_t)n + 1, _Alignof(char));
    if (!buf)
        return SEQC_OOM;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    seqc_status_t st = SEQC_OK;
    for (int i = 0; i < n; i++)
    {
        st = strbuf_append_char(sb, buf[i]);
        if (st != SEQC_OK)
            break;
    }
    if (buf != stack_buf)
        mem_free(sb->allocator, buf, (size_t)n + 1);
    return st;
}

/* string_join is defined after the builder because it uses strbuf_t. */
string_t string_join(iter_t it, string_t sep, allocator_t allocator)
{
    strbuf_t *sb = strbuf_create(allocator);
    string_t token;
    bool first = true;
    while (it.next(&it, &token))
    {
        if (!first)
            strbuf_append(sb, sep);
        strbuf_append(sb, token);
        first = false;
    }
    iter_drop(&it);
    return strbuf_finish(sb);
}

/* --- string_to_int ------------------------------------------------------ */

bool string_to_int(string_t s, long long *out)
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

bool string_to_double(string_t s, double *out)
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

/* --- iter_t sources ------------------------------------------------------- */

iter_t string_chars(string_t s, allocator_t allocator)
{
    slice_t sl = {(void *)s.ptr, s.len, sizeof(char)};
    return iter_from_slice(sl, allocator);
}

iter_t string_chars_rev(string_t s, allocator_t allocator)
{
    slice_t sl = {(void *)s.ptr, s.len, sizeof(char)};
    return iter_from_slice_rev(sl, allocator);
}

/* ---- string_split_substr / string_split_any ---------------------------- */

typedef struct
{
    string_t src;
    string_t delim; /* the substring to match, or the char set         */
    size_t pos;
    bool anychar; /* true: delim is a set, every matching char splits */
} split_state_t;

/* Index of the first byte of s that appears in set, or STRING_NOT_FOUND. */
static size_t charset_find(string_t s, string_t set)
{
    for (size_t i = 0; i < s.len; i++)
        for (size_t j = 0; j < set.len; j++)
            if (s.ptr[i] == set.ptr[j])
                return i;
    return STRING_NOT_FOUND;
}

static bool split_next(iter_t *it, void *out)
{
    split_state_t *s = it->state;
    if (s->pos > s->src.len)
        return false;

    string_t remaining = {s->src.ptr + s->pos, s->src.len - s->pos};

    /* Locate the next boundary.  An empty delim/set matches nowhere, so the
     * whole remaining string becomes a single token (use string_chars for
     * per-character iteration). */
    size_t off = STRING_NOT_FOUND;
    size_t blen = s->delim.len;
    if (s->delim.len > 0)
    {
        if (s->anychar)
        {
            off = charset_find(remaining, s->delim);
            blen = 1; /* each matching char is its own boundary */
        }
        else
        {
            off = string_find(remaining, s->delim);
        }
    }

    string_t token;
    if (off == STRING_NOT_FOUND)
    {
        token = remaining;
        s->pos = s->src.len + 1; /* mark exhausted */
    }
    else
    {
        token = (string_t){remaining.ptr, off};
        s->pos += off + blen;
    }

    memcpy(out, &token, sizeof(string_t));
    return true;
}

static void split_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(split_state_t));
}

static iter_t split_make(
    string_t s, string_t delim, bool anychar, allocator_t allocator)
{
    split_state_t *state =
        mem_alloc(allocator, sizeof(split_state_t), _Alignof(split_state_t));
    if (!state)
        return (iter_t){0};
    *state = (split_state_t){s, delim, 0, anychar};
    return (iter_t){.next = split_next,
                    .drop = split_drop,
                    .state = state,
                    .elem_size = sizeof(string_t),
                    .allocator = allocator};
}

iter_t string_split_substr(string_t s, string_t delim, allocator_t allocator)
{
    return split_make(s, delim, false, allocator);
}

iter_t string_split_any(string_t s, string_t set, allocator_t allocator)
{
    return split_make(s, set, true, allocator);
}

/* --- hashmap_t helpers ---------------------------------------------------- */

size_t string_hash(const void *key, size_t key_size)
{
    (void)key_size;
    if (!key)
        return 0;
    const string_t *s = (const string_t *)key;
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
    return string_equals(*(const string_t *)a, *(const string_t *)b);
}
