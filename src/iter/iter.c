#include "seqc/iter.h"
#include "arena/allocator.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define SEQC_SCRATCH_MAX 512

/* Returns stack_buf if elem_size fits, otherwise allocates from al. */
static inline void *scratch_acquire(
    char *stack_buf, size_t elem_size, allocator_t al)
{
    if (elem_size <= SEQC_SCRATCH_MAX)
        return stack_buf;
    return mem_alloc(al, elem_size, _Alignof(max_align_t));
}

static inline void scratch_release(
    char *stack_buf, void *ptr, allocator_t al, size_t size)
{
    if (ptr != stack_buf)
        mem_free(al, ptr, size);
}

/* ---- iter_from_slice -------------------------------------------------- */

typedef struct
{
    const char *ptr;
    size_t len;
    size_t elem_size;
    size_t pos;
} slice_iter_state_t;

static bool slice_next(iter_t *it, void *out)
{
    if (!it || !it->state || !out)
    {
        return false; /* invalid iterator or output buffer */
    }

    slice_iter_state_t *s = it->state;
    if (s->pos >= s->len)
        return false;
    memcpy(out, s->ptr + s->pos * s->elem_size, s->elem_size);
    s->pos++;
    return true;
}

static void slice_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(slice_iter_state_t));
}

iter_t iter_from_slice(slice_t s, allocator_t allocator)
{
    slice_iter_state_t *state =
        mem_alloc(allocator, sizeof *state, _Alignof(slice_iter_state_t));
    if (!state)
        return (iter_t){0};
    *state = (slice_iter_state_t){s.ptr, s.len, s.elem_size, 0};
    return (iter_t){.next = slice_next,
                  .drop = slice_drop,
                  .state = state,
                  .elem_size = s.elem_size,
                  .allocator = allocator};
}

static bool slice_rev_next(iter_t *it, void *out)
{
    slice_iter_state_t *s = it->state;
    if (!s->pos)
        return false;
    s->pos--;
    memcpy(out, s->ptr + s->pos * s->elem_size, s->elem_size);
    return true;
}

iter_t iter_from_slice_rev(slice_t s, allocator_t allocator)
{
    slice_iter_state_t *state =
        mem_alloc(allocator, sizeof *state, _Alignof(slice_iter_state_t));
    if (!state)
        return (iter_t){0};
    *state = (slice_iter_state_t){s.ptr, s.len, s.elem_size, s.len};
    return (iter_t){.next = slice_rev_next,
                  .drop = slice_drop,
                  .state = state,
                  .elem_size = s.elem_size,
                  .allocator = allocator};
}

/* ---- iter_generate ----------------------------------------------------- */

typedef struct
{
    generate_fn fn;
    void *ctx;
} generate_state_t;

static bool generate_next(iter_t *it, void *out)
{
    generate_state_t *s = it->state;
    return s->fn(out, s->ctx);
}

static void generate_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(generate_state_t));
}

iter_t iter_generate(
    generate_fn fn, void *ctx, size_t elem_size, allocator_t allocator)
{
    generate_state_t *s = mem_alloc(allocator, sizeof *s, _Alignof(generate_state_t));
    if (!s)
        return (iter_t){0};
    *s = (generate_state_t){fn, ctx};
    return (iter_t){.next = generate_next,
                  .drop = generate_drop,
                  .state = s,
                  .elem_size = elem_size,
                  .allocator = allocator};
}

/* ---- iter_range -------------------------------------------------------- */

typedef struct
{
    long long cur;
    long long end;
    long long step;
} range_state_t;

static bool range_next(iter_t *it, void *out)
{
    range_state_t *s = it->state;
    if (s->step > 0 && s->cur >= s->end)
        return false;
    if (s->step < 0 && s->cur <= s->end)
        return false;
    memcpy(out, &s->cur, sizeof(long long));
    s->cur += s->step;
    return true;
}

static void range_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(range_state_t));
}

iter_t iter_range(
    long long start, long long end, long long step, allocator_t allocator)
{
    if (step == 0)
    {
        /* step=0 would loop forever; return an immediately-empty range */
        range_state_t *s = mem_alloc(allocator, sizeof *s, _Alignof(range_state_t));
        if (!s)
            return (iter_t){0};
        *s = (range_state_t){start, start, 1};
        return (iter_t){.next = range_next,
                      .drop = range_drop,
                      .state = s,
                      .elem_size = sizeof(long long),
                      .allocator = allocator};
    }
    range_state_t *s = mem_alloc(allocator, sizeof *s, _Alignof(range_state_t));
    if (!s)
        return (iter_t){0};
    *s = (range_state_t){start, end, step};
    return (iter_t){.next = range_next,
                  .drop = range_drop,
                  .state = s,
                  .elem_size = sizeof(long long),
                  .allocator = allocator};
}

/* ---- iter_filter ------------------------------------------------------- */

typedef struct
{
    iter_t source;
    pred_fn pred;
    void *ctx;
} filter_state_t;

static bool filter_next(iter_t *it, void *out)
{
    if (!it || !it->state || !out)
    {
        return false; /* invalid iterator or output buffer */
    }
    filter_state_t *s = it->state;
    while (s->source.next(&s->source, out))
        if (s->pred(out, s->ctx))
            return true;
    return false;
}

static void filter_drop(iter_t *it)
{
    if (!it->state)
        return;
    filter_state_t *s = it->state;
    iter_drop(&s->source);
    mem_free(it->allocator, s, sizeof(filter_state_t));
}

iter_t iter_filter(iter_t source, pred_fn pred, void *ctx)
{
    filter_state_t *s =
        mem_alloc(source.allocator, sizeof *s, _Alignof(filter_state_t));
    if (!s)
        return (iter_t){0};
    *s = (filter_state_t){source, pred, ctx};
    return (iter_t){.next = filter_next,
                  .drop = filter_drop,
                  .state = s,
                  .elem_size = source.elem_size,
                  .allocator = source.allocator};
}

/* ---- iter_map ---------------------------------------------------------- */

typedef struct
{
    iter_t source;
    map_fn map;
    void *ctx;
    void *in_buf; /* reusable buffer of source.elem_size bytes */
} map_state_t;

static bool map_next(iter_t *it, void *out)
{
    if (!it || !it->state || !out)
    {
        return false; /* invalid iterator or output buffer */
    }
    map_state_t *s = it->state;
    if (!s->source.next(&s->source, s->in_buf))
        return false;
    s->map(s->in_buf, out, s->ctx);
    return true;
}

static void map_drop(iter_t *it)
{
    if (!it->state)
        return;
    map_state_t *s = it->state;
    iter_drop(&s->source);
    mem_free(it->allocator, s->in_buf, s->source.elem_size);
    mem_free(it->allocator, s, sizeof(map_state_t));
}

iter_t iter_map(iter_t source, map_fn map, void *ctx, size_t out_elem_size)
{
    map_state_t *s = mem_alloc(source.allocator, sizeof *s, _Alignof(map_state_t));
    if (!s)
        return (iter_t){0};
    void *in_buf =
        mem_alloc(source.allocator, source.elem_size, _Alignof(max_align_t));
    if (!in_buf)
    {
        mem_free(source.allocator, s, sizeof(map_state_t));
        return (iter_t){0};
    }
    *s = (map_state_t){source, map, ctx, in_buf};
    return (iter_t){.next = map_next,
                  .drop = map_drop,
                  .state = s,
                  .elem_size = out_elem_size,
                  .allocator = source.allocator};
}

/* ---- iter_take --------------------------------------------------------- */

typedef struct
{
    iter_t source;
    size_t remaining;
} take_state_t;

static bool take_next(iter_t *it, void *out)
{
    take_state_t *s = it->state;
    if (s->remaining == 0)
        return false;
    if (!s->source.next(&s->source, out))
        return false;
    s->remaining--;
    return true;
}

static void take_drop(iter_t *it)
{
    if (!it->state)
        return;
    take_state_t *s = it->state;
    iter_drop(&s->source);
    mem_free(it->allocator, s, sizeof(take_state_t));
}

iter_t iter_take(iter_t source, size_t n)
{
    take_state_t *s = mem_alloc(source.allocator, sizeof *s, _Alignof(take_state_t));
    if (!s)
        return (iter_t){0};
    *s = (take_state_t){source, n};
    return (iter_t){.next = take_next,
                  .drop = take_drop,
                  .state = s,
                  .elem_size = source.elem_size,
                  .allocator = source.allocator};
}

/* ---- iter_take_while ---------------------------------------------------- */

typedef struct
{
    iter_t source;
    pred_fn pred;
    void *ctx;
} take_while_state_t;

static bool take_while_next(iter_t *it, void *out)
{
    take_while_state_t *s = it->state;
    if (!s->source.next(&s->source, out))
        return false;
    if (!s->pred(out, s->ctx))
        return false;
    return true;
}

static void take_while_drop(iter_t *it)
{
    if (!it->state)
        return;
    take_while_state_t *s = it->state;
    iter_drop(&s->source);
    mem_free(it->allocator, s, sizeof(take_while_state_t));
}

iter_t iter_take_while(iter_t source, pred_fn pred, void *ctx)
{
    take_while_state_t *s =
        mem_alloc(source.allocator, sizeof *s, _Alignof(take_while_state_t));
    if (!s)
        return (iter_t){0};
    *s = (take_while_state_t){source, pred, ctx};
    return (iter_t){.next = take_while_next,
                  .drop = take_while_drop,
                  .state = s,
                  .elem_size = source.elem_size,
                  .allocator = source.allocator};
}

/* ---- iter_skip --------------------------------------------------------- */

typedef struct
{
    iter_t source;
    size_t remaining;
} skip_state_t;

static bool skip_next(iter_t *it, void *out)
{
    skip_state_t *s = it->state;
    /* drain skipped elements once, then pass through */
    while (s->remaining > 0)
    {
        if (!s->source.next(&s->source, out))
            return false;
        s->remaining--;
    }
    return s->source.next(&s->source, out);
}

static void skip_drop(iter_t *it)
{
    if (!it->state)
        return;
    skip_state_t *s = it->state;
    iter_drop(&s->source);
    mem_free(it->allocator, s, sizeof(skip_state_t));
}

iter_t iter_skip(iter_t source, size_t n)
{
    skip_state_t *s = mem_alloc(source.allocator, sizeof *s, _Alignof(skip_state_t));
    if (!s)
        return (iter_t){0};
    *s = (skip_state_t){source, n};
    return (iter_t){.next = skip_next,
                  .drop = skip_drop,
                  .state = s,
                  .elem_size = source.elem_size,
                  .allocator = source.allocator};
}

/* ---- iter_skip_while ---------------------------------------------------- */

typedef struct
{
    iter_t source;
    pred_fn pred;
    void *ctx;
    bool done_skipping;
} skip_while_state_t;

static bool skip_while_next(iter_t *it, void *out)
{
    skip_while_state_t *s = it->state;
    while (s->source.next(&s->source, out))
    {
        if (s->done_skipping || !s->pred(out, s->ctx))
        {
            s->done_skipping = true;
            return true;
        }
    }
    return false;
}

static void skip_while_drop(iter_t *it)
{
    if (!it->state)
        return;
    skip_while_state_t *s = it->state;
    iter_drop(&s->source);
    mem_free(it->allocator, s, sizeof(skip_while_state_t));
}

iter_t iter_skip_while(iter_t source, pred_fn pred, void *ctx)
{
    skip_while_state_t *s =
        mem_alloc(source.allocator, sizeof *s, _Alignof(skip_while_state_t));
    if (!s)
        return (iter_t){0};
    *s = (skip_while_state_t){source, pred, ctx, false};
    return (iter_t){.next = skip_while_next,
                  .drop = skip_while_drop,
                  .state = s,
                  .elem_size = source.elem_size,
                  .allocator = source.allocator};
}

/* ---- terminals --------------------------------------------------------- */

slice_t iter_collect(iter_t it, allocator_t allocator)
{
    const size_t elem_size = it.elem_size;
    size_t cap = 16;
    size_t len = 0;

    char *buf = mem_alloc(allocator, cap * elem_size, _Alignof(max_align_t));
    if (!buf)
    {
        iter_drop(&it);
        return (slice_t){NULL, 0, elem_size};
    }

    char sbuf[SEQC_SCRATCH_MAX];
    void *tmp = scratch_acquire(sbuf, elem_size, allocator);
    if (!tmp)
    {
        iter_drop(&it);
        mem_free(allocator, buf, cap * elem_size);
        return (slice_t){NULL, 0, elem_size};
    }

    while (it.next(&it, tmp))
    {
        if (len == cap)
        {
            size_t new_cap = cap * 2;
            char *grown = mem_realloc(
                allocator,

                buf,
                cap * elem_size,
                new_cap * elem_size,
                _Alignof(max_align_t));
            if (!grown)
            {
                /* return what we collected so far rather than losing it */
                break;
            }
            buf = grown;
            cap = new_cap;
        }
        memcpy(buf + len * elem_size, tmp, elem_size);
        len++;
    }
    iter_drop(&it);
    scratch_release(sbuf, tmp, allocator, elem_size);

    if (len == 0)
    {
        mem_free(allocator, buf, cap * elem_size);
        return (slice_t){NULL, 0, elem_size};
    }

    /* Shrink to fit.
     * Arena: arena_realloc returns buf unchanged (new_size <= old_size is a
     * no-op). sys_allocator: realloc actually shrinks the allocation. */
    if (len < cap)
    {
        void *tight = mem_realloc(
            allocator,

            buf,
            cap * elem_size,
            len * elem_size,
            _Alignof(max_align_t));
        if (tight)
            buf = tight;
    }
    return (slice_t){buf, len, elem_size};
}

size_t iter_count(iter_t it)
{
    char sbuf[SEQC_SCRATCH_MAX];
    void *tmp = scratch_acquire(sbuf, it.elem_size, it.allocator);
    if (!tmp)
    {
        iter_drop(&it);
        return 0;
    }
    size_t n = 0;
    while (it.next(&it, tmp))
        n++;
    iter_drop(&it);
    scratch_release(sbuf, tmp, it.allocator, it.elem_size);
    return n;
}

void iter_foreach(iter_t it, visitor_fn visit, void *ctx)
{
    char sbuf[SEQC_SCRATCH_MAX];
    void *tmp = scratch_acquire(sbuf, it.elem_size, it.allocator);
    if (!tmp)
    {
        iter_drop(&it);
        return;
    }
    while (it.next(&it, tmp))
        visit(tmp, ctx);
    iter_drop(&it);
    scratch_release(sbuf, tmp, it.allocator, it.elem_size);
}

void iter_reduce(iter_t it, void *acc, combine_fn combine, void *ctx)
{
    char sbuf[SEQC_SCRATCH_MAX];
    void *tmp = scratch_acquire(sbuf, it.elem_size, it.allocator);
    if (!tmp)
    {
        iter_drop(&it);
        return;
    }
    while (it.next(&it, tmp))
        combine(acc, tmp, ctx);
    iter_drop(&it);
    scratch_release(sbuf, tmp, it.allocator, it.elem_size);
}

/* ---- iter_chain -------------------------------------------------------- */

typedef struct
{
    iter_t first;
    iter_t second;
    int done_first;
} chain_state_t;

static bool chain_next(iter_t *it, void *out)
{
    chain_state_t *s = it->state;
    if (!s->done_first)
    {
        if (s->first.next(&s->first, out))
            return true;
        s->done_first = 1;
    }
    return s->second.next(&s->second, out);
}

static void chain_drop(iter_t *it)
{
    if (!it->state)
        return;
    chain_state_t *s = it->state;
    iter_drop(&s->first);
    iter_drop(&s->second);
    mem_free(it->allocator, s, sizeof(chain_state_t));
}

iter_t iter_chain(iter_t a, iter_t b)
{
    chain_state_t *s = mem_alloc(a.allocator, sizeof *s, _Alignof(chain_state_t));
    if (!s)
    {
        return (iter_t){0};
    }
    *s = (chain_state_t){a, b, 0};
    return (iter_t){.next = chain_next,
                  .drop = chain_drop,
                  .state = s,
                  .elem_size = a.elem_size,
                  .allocator = a.allocator};
}

/* ---- iter_zip ---------------------------------------------------------- */

typedef struct
{
    iter_t a;
    iter_t b;
    size_t a_elem_size;
    void *buf_a; /* temporary buffer; confirms a before writing b to out */
} zip_state_t;

static bool zip_next(iter_t *it, void *out)
{
    zip_state_t *s = it->state;
    if (!s->a.next(&s->a, s->buf_a))
        return false;
    if (!s->b.next(&s->b, (char *)out + s->a_elem_size))
        return false;
    memcpy(out, s->buf_a, s->a_elem_size);
    return true;
}

static void zip_drop(iter_t *it)
{
    if (!it->state)
        return;
    zip_state_t *s = it->state;
    iter_drop(&s->a);
    iter_drop(&s->b);
    mem_free(it->allocator, s->buf_a, s->a_elem_size);
    mem_free(it->allocator, s, sizeof(zip_state_t));
}

iter_t iter_zip(iter_t a, iter_t b)
{
    zip_state_t *s = mem_alloc(a.allocator, sizeof *s, _Alignof(zip_state_t));
    void *buf_a = mem_alloc(a.allocator, a.elem_size, _Alignof(max_align_t));
    if (!s || !buf_a)
    {
        if (s)
            mem_free(a.allocator, s, sizeof(zip_state_t));
        if (buf_a)
            mem_free(a.allocator, buf_a, a.elem_size);
        return (iter_t){0};
    }
    *s = (zip_state_t){a, b, a.elem_size, buf_a};
    return (iter_t){.next = zip_next,
                  .drop = zip_drop,
                  .state = s,
                  .elem_size = a.elem_size + b.elem_size,
                  .allocator = a.allocator};
}

/* ---- iter_sort --------------------------------------------------------- */

slice_t iter_sort(iter_t it, compare_fn cmp, allocator_t allocator)
{
    slice_t s = iter_collect(it, allocator);
    if (s.len > 1)
        qsort(s.ptr, s.len, s.elem_size, cmp);
    return s;
}

/* ---- iter_find --------------------------------------------------------- */

bool iter_find(iter_t it, pred_fn pred, void *ctx, void *out)
{
    char sbuf[SEQC_SCRATCH_MAX];
    void *tmp = scratch_acquire(sbuf, it.elem_size, it.allocator);
    if (!tmp)
    {
        iter_drop(&it);
        return false;
    }
    bool found = false;
    while (it.next(&it, tmp))
    {
        if (pred(tmp, ctx))
        {
            if (out)
                memcpy(out, tmp, it.elem_size);
            found = true;
            break;
        }
    }
    iter_drop(&it);
    scratch_release(sbuf, tmp, it.allocator, it.elem_size);
    return found;
}

/* ---- iter_any ---------------------------------------------------------- */

bool iter_any(iter_t it, pred_fn pred, void *ctx)
{
    char sbuf[SEQC_SCRATCH_MAX];
    void *tmp = scratch_acquire(sbuf, it.elem_size, it.allocator);
    if (!tmp)
    {
        iter_drop(&it);
        return false;
    }
    bool found = false;
    while (!found && it.next(&it, tmp))
        if (pred(tmp, ctx))
            found = true;
    iter_drop(&it);
    scratch_release(sbuf, tmp, it.allocator, it.elem_size);
    return found;
}

/* ---- iter_all ---------------------------------------------------------- */

bool iter_all(iter_t it, pred_fn pred, void *ctx)
{
    char sbuf[SEQC_SCRATCH_MAX];
    void *tmp = scratch_acquire(sbuf, it.elem_size, it.allocator);
    if (!tmp)
    {
        iter_drop(&it);
        return false;
    }
    bool all = true;
    while (all && it.next(&it, tmp))
        if (!pred(tmp, ctx))
            all = false;
    iter_drop(&it);
    scratch_release(sbuf, tmp, it.allocator, it.elem_size);
    return all;
}

/* ---- iter_enumerate ---------------------------------------------------- */

typedef struct
{
    iter_t source;
    size_t index;
    void *buf; /* holds the current element so enum_entry_t.elem is valid */
} enum_state_t;

static bool enum_next(iter_t *it, void *out)
{
    enum_state_t *s = it->state;
    if (!s->source.next(&s->source, s->buf))
        return false;
    enum_entry_t entry = {s->index++, s->buf};
    memcpy(out, &entry, sizeof(enum_entry_t));
    return true;
}

static void enum_drop(iter_t *it)
{
    enum_state_t *s = it->state;
    iter_drop(&s->source);
    mem_free(it->allocator, s->buf, s->source.elem_size);
    mem_free(it->allocator, s, sizeof(enum_state_t));
}

iter_t iter_enumerate(iter_t source)
{
    enum_state_t *s = mem_alloc(source.allocator, sizeof *s, _Alignof(enum_state_t));
    void *buf =
        mem_alloc(source.allocator, source.elem_size, _Alignof(max_align_t));
    if (!s || !buf)
    {
        if (s)
            mem_free(source.allocator, s, sizeof(enum_state_t));
        if (buf)
            mem_free(source.allocator, buf, source.elem_size);
        return (iter_t){0};
    }
    *s = (enum_state_t){source, 0, buf};
    return (iter_t){.next = enum_next,
                  .drop = enum_drop,
                  .state = s,
                  .elem_size = sizeof(enum_entry_t),
                  .allocator = source.allocator};
}

/* ---- iter_window ------------------------------------------------------- */

typedef struct
{
    iter_t source;
    char *buf; /* n * elem_size bytes                  */
    size_t n;
    size_t elem_size;
    int primed; /* 1 once the first window is filled    */
    allocator_t allocator;
} window_state_t;

static bool window_next(iter_t *it, void *out)
{
    window_state_t *s = it->state;
    if (s->n == 0)
        return false; /* a zero-width window yields nothing */
    if (!s->primed)
    {
        /* Fill first n elements */
        for (size_t i = 0; i < s->n; i++)
        {
            if (!s->source.next(&s->source, s->buf + i * s->elem_size))
                return false; /* source shorter than n */
        }
        s->primed = 1;
    }
    else
    {
        /* Shift left by one, read new element at end */
        memmove(s->buf, s->buf + s->elem_size, (s->n - 1) * s->elem_size);
        if (!s->source.next(&s->source, s->buf + (s->n - 1) * s->elem_size))
            return false;
    }
    slice_t sl = {s->buf, s->n, s->elem_size};
    memcpy(out, &sl, sizeof(slice_t));
    return true;
}

static void window_drop(iter_t *it)
{
    window_state_t *s = it->state;
    iter_drop(&s->source);
    if (s->buf)
        mem_free(it->allocator, s->buf, s->n * s->elem_size);
    mem_free(it->allocator, s, sizeof(window_state_t));
}

iter_t iter_window(iter_t source, size_t n)
{
    window_state_t *s =
        mem_alloc(source.allocator, sizeof *s, _Alignof(window_state_t));
    /* n == 0 needs no buffer; window_next yields nothing in that case */
    char *buf = n ? mem_alloc(source.allocator, n * source.elem_size,
                              _Alignof(max_align_t))
                  : NULL;
    if (!s || (n && !buf))
    {
        if (s)
            mem_free(source.allocator, s, sizeof(window_state_t));
        if (buf)
            mem_free(source.allocator, buf, n * source.elem_size);
        return (iter_t){0};
    }
    *s = (window_state_t){source, buf, n, source.elem_size, 0, source.allocator};
    return (iter_t){.next = window_next,
                  .drop = window_drop,
                  .state = s,
                  .elem_size = sizeof(slice_t),
                  .allocator = source.allocator};
}

/* ---- iter_chunks ------------------------------------------------------- */

typedef struct
{
    iter_t source;
    char *buf; /* n * elem_size bytes */
    size_t n;
    size_t elem_size;
    int done;
    allocator_t allocator;
} chunk_state_t;

static bool chunk_next(iter_t *it, void *out)
{
    chunk_state_t *s = it->state;
    if (s->done)
        return false;
    size_t count = 0;
    while (count < s->n
           && s->source.next(&s->source, s->buf + count * s->elem_size))
        count++;
    if (count == 0)
        return false;
    if (count < s->n)
        s->done = 1; /* last partial chunk */
    slice_t sl = {s->buf, count, s->elem_size};
    memcpy(out, &sl, sizeof(slice_t));
    return true;
}

static void chunk_drop(iter_t *it)
{
    chunk_state_t *s = it->state;
    iter_drop(&s->source);
    if (s->buf)
        mem_free(it->allocator, s->buf, s->n * s->elem_size);
    mem_free(it->allocator, s, sizeof(chunk_state_t));
}

iter_t iter_chunks(iter_t source, size_t n)
{
    chunk_state_t *s =
        mem_alloc(source.allocator, sizeof *s, _Alignof(chunk_state_t));
    /* n == 0 needs no buffer; chunk_next yields nothing in that case */
    char *buf = n ? mem_alloc(source.allocator, n * source.elem_size,
                              _Alignof(max_align_t))
                  : NULL;
    if (!s || (n && !buf))
    {
        if (s)
            mem_free(source.allocator, s, sizeof(chunk_state_t));
        if (buf)
            mem_free(source.allocator, buf, n * source.elem_size);
        return (iter_t){0};
    }
    *s = (chunk_state_t){source, buf, n, source.elem_size, 0, source.allocator};
    return (iter_t){.next = chunk_next,
                  .drop = chunk_drop,
                  .state = s,
                  .elem_size = sizeof(slice_t),
                  .allocator = source.allocator};
}

/* ---- iter_peekable ----------------------------------------------------- */

typedef struct
{
    iter_t source;
    void *buf;
    bool has_peeked;
    bool source_done;
} peekable_state_t;

static bool peekable_next(iter_t *it, void *out)
{
    peekable_state_t *s = it->state;
    if (s->has_peeked)
    {
        memcpy(out, s->buf, it->elem_size);
        s->has_peeked = false;
        return true;
    }
    if (s->source_done)
        return false;
    return s->source.next(&s->source, out);
}

static bool peekable_peek(iter_t *it, void *out)
{
    peekable_state_t *s = it->state;
    if (!s->has_peeked)
    {
        if (s->source_done)
            return false;
        if (!s->source.next(&s->source, s->buf))
        {
            s->source_done = true;
            return false;
        }
        s->has_peeked = true;
    }
    if (out)
        memcpy(out, s->buf, it->elem_size);
    return true;
}

static void peekable_drop(iter_t *it)
{
    if (!it->state)
        return;
    peekable_state_t *s = it->state;
    iter_drop(&s->source);
    mem_free(it->allocator, s->buf, it->elem_size);
    mem_free(it->allocator, s, sizeof(peekable_state_t));
}

iter_t iter_peekable(iter_t source)
{
    peekable_state_t *s =
        mem_alloc(source.allocator, sizeof *s, _Alignof(peekable_state_t));
    void *buf =
        mem_alloc(source.allocator, source.elem_size, _Alignof(max_align_t));
    if (!s || !buf)
    {
        if (s)
            mem_free(source.allocator, s, sizeof(peekable_state_t));
        if (buf)
            mem_free(source.allocator, buf, source.elem_size);
        return (iter_t){0};
    }
    *s = (peekable_state_t){source, buf, false, false};
    return (iter_t){.next = peekable_next,
                  .drop = peekable_drop,
                  .peek = peekable_peek,
                  .state = s,
                  .elem_size = source.elem_size,
                  .allocator = source.allocator};
}

/* ---- iter_dedup -------------------------------------------------------- */

typedef struct
{
    iter_t source;
    compare_fn cmp;
    void *last_buf;
    bool has_last;
} dedup_state_t;

static bool dedup_next(iter_t *it, void *out)
{
    dedup_state_t *s = it->state;
    while (s->source.next(&s->source, out))
    {
        if (!s->has_last || s->cmp(out, s->last_buf) != 0)
        {
            memcpy(s->last_buf, out, it->elem_size);
            s->has_last = true;
            return true;
        }
    }
    return false;
}

static void dedup_drop(iter_t *it)
{
    if (!it->state)
        return;
    dedup_state_t *s = it->state;
    iter_drop(&s->source);
    mem_free(it->allocator, s->last_buf, it->elem_size);
    mem_free(it->allocator, s, sizeof(dedup_state_t));
}

iter_t iter_dedup(iter_t source, compare_fn cmp)
{
    dedup_state_t *s =
        mem_alloc(source.allocator, sizeof *s, _Alignof(dedup_state_t));
    void *last_buf =
        mem_alloc(source.allocator, source.elem_size, _Alignof(max_align_t));
    if (!s || !last_buf)
    {
        if (s)
            mem_free(source.allocator, s, sizeof(dedup_state_t));
        if (last_buf)
            mem_free(source.allocator, last_buf, source.elem_size);
        return (iter_t){0};
    }
    *s = (dedup_state_t){source, cmp, last_buf, false};
    return (iter_t){.next = dedup_next,
                  .drop = dedup_drop,
                  .state = s,
                  .elem_size = source.elem_size,
                  .allocator = source.allocator};
}

/* ---- iter_flat_map ----------------------------------------------------- */

typedef struct
{
    iter_t source; /* outer iterator                        */
    flat_map_fn fn;
    void *ctx;
    void *elem_buf; /* one element from source               */
    iter_t sub;       /* current sub-iterator (zeroed = none)  */
    int sub_active; /* 1 when sub holds a live iterator      */
    size_t out_elem_size;
    allocator_t allocator;
} flat_map_state_t;

static bool flat_map_next(iter_t *it, void *out)
{
    flat_map_state_t *s = it->state;
    while (1)
    {
        /* Drain current sub-iterator */
        if (s->sub_active)
        {
            if (s->sub.next(&s->sub, out))
                return true;
            iter_drop(&s->sub);
            s->sub_active = 0;
        }
        /* Pull next element from source */
        if (!s->source.next(&s->source, s->elem_buf))
            return false;
        /* Produce next sub-iterator */
        s->fn(s->elem_buf, &s->sub, s->ctx);
        s->sub_active = 1;
    }
}

static void flat_map_drop(iter_t *it)
{
    flat_map_state_t *s = it->state;
    if (s->sub_active)
        iter_drop(&s->sub);
    iter_drop(&s->source);
    mem_free(it->allocator, s->elem_buf, s->source.elem_size);
    mem_free(it->allocator, s, sizeof(flat_map_state_t));
}

iter_t iter_flat_map(iter_t source, flat_map_fn fn, void *ctx, size_t out_elem_size)
{
    flat_map_state_t *s =
        mem_alloc(source.allocator, sizeof *s, _Alignof(flat_map_state_t));
    void *elem_buf =
        mem_alloc(source.allocator, source.elem_size, _Alignof(max_align_t));
    if (!s || !elem_buf)
    {
        if (s)
            mem_free(source.allocator, s, sizeof(flat_map_state_t));
        if (elem_buf)
            mem_free(source.allocator, elem_buf, source.elem_size);
        return (iter_t){0};
    }
    *s = (flat_map_state_t){.source = source,
                        .fn = fn,
                        .ctx = ctx,
                        .elem_buf = elem_buf,
                        .sub = {0},
                        .sub_active = 0,
                        .out_elem_size = out_elem_size,
                        .allocator = source.allocator};
    return (iter_t){.next = flat_map_next,
                  .drop = flat_map_drop,
                  .state = s,
                  .elem_size = out_elem_size,
                  .allocator = source.allocator};
}

/* ---- iter_min / iter_max ----------------------------------------------- */

bool iter_min(iter_t it, compare_fn cmp, void *out)
{
    size_t elem_size = it.elem_size;
    char sbuf_best[SEQC_SCRATCH_MAX];
    char sbuf_cur[SEQC_SCRATCH_MAX];
    void *best = scratch_acquire(sbuf_best, elem_size, it.allocator);
    void *cur = scratch_acquire(sbuf_cur, elem_size, it.allocator);
    if (!best || !cur)
    {
        iter_drop(&it);
        scratch_release(sbuf_best, best, it.allocator, elem_size);
        scratch_release(sbuf_cur, cur, it.allocator, elem_size);
        return false;
    }
    bool found = false;
    while (it.next(&it, cur))
    {
        if (!found || cmp(cur, best) < 0)
            memcpy(best, cur, elem_size);
        found = true;
    }
    if (found && out)
        memcpy(out, best, elem_size);
    iter_drop(&it);
    scratch_release(sbuf_best, best, it.allocator, elem_size);
    scratch_release(sbuf_cur, cur, it.allocator, elem_size);
    return found;
}

bool iter_max(iter_t it, compare_fn cmp, void *out)
{
    size_t elem_size = it.elem_size;
    char sbuf_best[SEQC_SCRATCH_MAX];
    char sbuf_cur[SEQC_SCRATCH_MAX];
    void *best = scratch_acquire(sbuf_best, elem_size, it.allocator);
    void *cur = scratch_acquire(sbuf_cur, elem_size, it.allocator);
    if (!best || !cur)
    {
        iter_drop(&it);
        scratch_release(sbuf_best, best, it.allocator, elem_size);
        scratch_release(sbuf_cur, cur, it.allocator, elem_size);
        return false;
    }
    bool found = false;
    while (it.next(&it, cur))
    {
        if (!found || cmp(cur, best) > 0)
            memcpy(best, cur, elem_size);
        found = true;
    }
    if (found && out)
        memcpy(out, best, elem_size);
    iter_drop(&it);
    scratch_release(sbuf_best, best, it.allocator, elem_size);
    scratch_release(sbuf_cur, cur, it.allocator, elem_size);
    return found;
}
