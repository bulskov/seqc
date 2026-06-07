#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/slice.h"

/* --- Error / status codes ---------------------------------------------- */

typedef enum
{
    SEQC_OK = 0,    /* operation succeeded / element found    */
    SEQC_NOT_FOUND, /* element or key is absent               */
    SEQC_DUPLICATE, /* element already present (no-op insert) */
    SEQC_OOM,       /* allocator returned NULL                */
    SEQC_INVALID,   /* NULL or otherwise invalid argument     */
} seqc_status_t;

typedef struct iter_t iter_t;

typedef bool (*pred_fn)(const void *elem, void *ctx);
typedef void (*map_fn)(const void *in, void *out, void *ctx);
typedef void (*combine_fn)(void *acc, const void *elem, void *ctx);
typedef void (*visitor_fn)(const void *elem, void *ctx);
typedef int (*compare_fn)(const void *a, const void *b);
typedef size_t (*hash_fn)(const void *key, size_t key_size);
typedef bool (*eq_fn)(const void *a, const void *b, size_t key_size);

struct iter_t
{
    bool (*next)(
        iter_t *it,
        void *out);         /* writes elem_size bytes; returns true or false */
    void (*drop)(iter_t *it); /* frees internal state; NULL is valid     */
    bool (*peek)(iter_t *it, void *out); /* NULL if not peekable            */
    void *state;
    size_t elem_size;
    allocator_t allocator; /* used by adaptors that need to allocate state */
};

/* Inline drop — call to release resources without a terminal */
static inline void iter_drop(iter_t *it)
{
    if (it && it->drop)
        it->drop(it);
}

/* Look at the next element without consuming it.
 * Returns false if the iterator is exhausted or not peekable.
 * Only valid on iterators returned by iter_peekable. */
static inline bool iter_peek(iter_t *it, void *out)
{
    return it && it->peek && it->peek(it, out);
}

/* --- Sources ------------------------------------------------------------ */

iter_t iter_from_slice(slice_t s, allocator_t allocator);
iter_t iter_from_slice_rev(slice_t s, allocator_t allocator);

/* Calls fn(out, ctx) on each call to next; stops when fn returns false.
 * fn writes exactly elem_size bytes into out on each successful call. */
typedef bool (*generate_fn)(void *out, void *ctx);
iter_t iter_generate(
    generate_fn fn, void *ctx, size_t elem_size, allocator_t allocator);

/* Yields long long integers from start (inclusive) to end (exclusive) by
 * step.  step=0 yields an empty range.  Yields nothing if the range is empty
 * (e.g. start >= end with positive step). */
iter_t iter_range(
    long long start, long long end, long long step, allocator_t allocator);

/* --- Adaptors (take ownership of source) -------------------------------- */

iter_t iter_filter(iter_t source, pred_fn pred, void *ctx);

iter_t iter_map(iter_t source, map_fn map, void *ctx, size_t out_elem_size);

iter_t iter_take(iter_t source, size_t n);
iter_t iter_take_while(iter_t source, pred_fn pred, void *ctx);
iter_t iter_skip(iter_t source, size_t n);
iter_t iter_skip_while(iter_t source, pred_fn pred, void *ctx);

/* Yields all of a then all of b; elem_size must match */
iter_t iter_chain(iter_t a, iter_t b);

/* Yields flat [a_elem | b_elem] pairs; stops when either source ends.
 * elem_size of result == a.elem_size + b.elem_size */
iter_t iter_zip(iter_t a, iter_t b);

/* Pairs each element with its 0-based index.
 * Yields enum_entry_t {index, elem} where elem points into an internal buffer.
 * Do not store elem across calls. */
typedef struct
{
    size_t index;
    void *elem; /* pointer into internal buffer — valid until next call */
} enum_entry_t;

/* Pointer-pair yielded by hashmap_iter and omap_iter.
 * Both pointers are valid only for the duration of the current iteration step;
 * do not modify the map while iterating. */
typedef struct
{
    void *key;
    void *value;
} map_entry_t;

iter_t iter_enumerate(iter_t source);

/* Yields overlapping windows of size n as a slice_t.
 * The slice points into an internal buffer — copy if you need to keep it.
 * Yields nothing if the source has fewer than n elements. */
iter_t iter_window(iter_t source, size_t n);

/* Yields non-overlapping chunks of size n as a slice_t.
 * The last chunk may be smaller than n.
 * The slice points into an internal buffer — copy if you need to keep it. */
iter_t iter_chunks(iter_t source, size_t n);

/* For each element in source, calls fn(elem, &sub, ctx) which must populate
 * *sub with a sub-iterator.  All sub-iterators are drained in order.
 * out_elem_size must match the elem_size of every sub-iterator produced. */
typedef void (*flat_map_fn)(const void *elem, iter_t *out, void *ctx);
iter_t iter_flat_map(
    iter_t source, flat_map_fn fn, void *ctx, size_t out_elem_size);

/* Wraps source so that iter_peek() can inspect the next element without
 * consuming it.  iter_next() on a peekable iterator consumes the buffered
 * element first. */
iter_t iter_peekable(iter_t source);

/* Skips consecutive duplicate elements according to cmp.
 * Two adjacent elements are considered duplicates when cmp returns 0.
 * Useful after iter_sort to yield only unique values. */
iter_t iter_dedup(iter_t source, compare_fn cmp);

/* --- Terminals (consume and drop the iterator) -------------------------- */

/* Materialise the iterator into an arena-owned slice_t.
 * `allocator` determines which arena owns the returned memory. */
slice_t iter_collect(iter_t it, allocator_t allocator);
size_t iter_count(iter_t it);
void iter_foreach(iter_t it, visitor_fn visit, void *ctx);
void iter_reduce(iter_t it, void *acc, combine_fn combine, void *ctx);

/* Collects into `allocator` and sorts in-place using cmp; returns sorted slice_t
 */
slice_t iter_sort(iter_t it, compare_fn cmp, allocator_t allocator);

/* Returns 1 and writes first match to *out (may be NULL) if found */
bool iter_find(iter_t it, pred_fn pred, void *ctx, void *out);

/* Returns 1 if any element satisfies pred */
bool iter_any(iter_t it, pred_fn pred, void *ctx);

/* Returns 1 if all elements satisfy pred (vacuously true for empty) */
bool iter_all(iter_t it, pred_fn pred, void *ctx);

/* Write the minimum element to *out; returns 1 on success, 0 if empty.
 * out may be NULL to simply test for emptiness. */
bool iter_min(iter_t it, compare_fn cmp, void *out);

/* Write the maximum element to *out; returns 1 on success, 0 if empty.
 * out may be NULL to simply test for emptiness. */
bool iter_max(iter_t it, compare_fn cmp, void *out);
