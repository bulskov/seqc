#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/arena.h"
#include "slice/slice.h"

/* --- Error / status codes ---------------------------------------------- */

typedef enum
{
    SEQC_OK = 0,       /* operation succeeded / element found    */
    SEQC_NOT_FOUND,    /* element or key is absent               */
    SEQC_DUPLICATE,    /* element already present (no-op insert) */
    SEQC_OOM,          /* allocator returned NULL                */
    SEQC_INVALID,      /* NULL or otherwise invalid argument     */
} SeqcStatus;

typedef struct Iter Iter;

typedef bool (*pred_fn)(const void *elem, void *ctx);
typedef void (*map_fn)(const void *in, void *out, void *ctx);
typedef void (*combine_fn)(void *acc, const void *elem, void *ctx);
typedef void (*visitor_fn)(const void *elem, void *ctx);
typedef int (*compare_fn)(const void *a, const void *b);
typedef size_t (*hash_fn)(const void *key, size_t key_size);
typedef bool (*eq_fn)(const void *a, const void *b, size_t key_size);

struct Iter
{
    bool (*next)(
        Iter *it,
        void *out);         /* writes elem_size bytes; returns true or false */
    void (*drop)(Iter *it); /* frees internal state; NULL is valid     */
    void *state;
    size_t elem_size;
    Allocator allocator; /* used by adaptors that need to allocate state */
};

/* Inline drop — call to release resources without a terminal */
static inline void iter_drop(Iter *it)
{
    if (it && it->drop)
        it->drop(it);
}

/* --- Sources ------------------------------------------------------------ */

Iter iter_from_slice(Slice s, Allocator allocator);
Iter iter_from_slice_rev(Slice s, Allocator allocator);

/* Calls fn(out, ctx) on each call to next; stops when fn returns false.
 * fn writes exactly elem_size bytes into out on each successful call. */
typedef bool (*generate_fn)(void *out, void *ctx);
Iter iter_generate(
    generate_fn fn, void *ctx, size_t elem_size, Allocator allocator);

/* Yields long long integers from start (inclusive) to end (exclusive) by
 * step.  step must not be zero.  Yields nothing if the range is empty
 * (e.g. start >= end with positive step). */
Iter iter_range(
    long long start, long long end, long long step, Allocator allocator);

/* --- Adaptors (take ownership of source) -------------------------------- */

Iter iter_filter(Iter source, pred_fn pred, void *ctx);

Iter iter_map(Iter source, map_fn map, void *ctx, size_t out_elem_size);

Iter iter_take(Iter source, size_t n);
Iter iter_take_while(Iter source, pred_fn pred, void *ctx);
Iter iter_skip(Iter source, size_t n);
Iter iter_skip_while(Iter source, pred_fn pred, void *ctx);

/* Yields all of a then all of b; elem_size must match */
Iter iter_chain(Iter a, Iter b);

/* Yields flat [a_elem | b_elem] pairs; stops when either source ends.
 * elem_size of result == a.elem_size + b.elem_size */
Iter iter_zip(Iter a, Iter b);

/* Pairs each element with its 0-based index.
 * Yields EnumEntry {index, elem} where elem points into an internal buffer.
 * Do not store elem across calls. */
typedef struct
{
    size_t index;
    void *elem; /* pointer into internal buffer — valid until next call */
} EnumEntry;

/* Pointer-pair yielded by hashmap_iter and omap_iter.
 * Both pointers are valid only for the duration of the current iteration step;
 * do not modify the map while iterating. */
typedef struct
{
    void *key;
    void *value;
} MapEntry;

Iter iter_enumerate(Iter source);

/* Yields overlapping windows of size n as a Slice.
 * The slice points into an internal buffer — copy if you need to keep it.
 * Yields nothing if the source has fewer than n elements. */
Iter iter_window(Iter source, size_t n);

/* Yields non-overlapping chunks of size n as a Slice.
 * The last chunk may be smaller than n.
 * The slice points into an internal buffer — copy if you need to keep it. */
Iter iter_chunks(Iter source, size_t n);

/* For each element in source, calls fn(elem, &sub, ctx) which must populate
 * *sub with a sub-iterator.  All sub-iterators are drained in order.
 * out_elem_size must match the elem_size of every sub-iterator produced. */
typedef void (*flat_map_fn)(const void *elem, Iter *out, void *ctx);
Iter iter_flat_map(
    Iter source, flat_map_fn fn, void *ctx, size_t out_elem_size);

/* --- Terminals (consume and drop the iterator) -------------------------- */

/* Materialise the iterator into an arena-owned Slice.
 * `allocator` determines which arena owns the returned memory. */
Slice iter_collect(Iter it, Allocator allocator);
size_t iter_count(Iter it);
void iter_foreach(Iter it, visitor_fn visit, void *ctx);
void iter_reduce(Iter it, void *acc, combine_fn combine, void *ctx);

/* Collects into `allocator` and sorts in-place using cmp; returns sorted Slice */
Slice iter_sort(Iter it, compare_fn cmp, Allocator allocator);

/* Returns 1 and writes first match to *out (may be NULL) if found */
bool iter_find(Iter it, pred_fn pred, void *ctx, void *out);

/* Returns 1 if any element satisfies pred */
bool iter_any(Iter it, pred_fn pred, void *ctx);

/* Returns 1 if all elements satisfy pred (vacuously true for empty) */
bool iter_all(Iter it, pred_fn pred, void *ctx);

/* Write the minimum element to *out; returns 1 on success, 0 if empty.
 * out may be NULL to simply test for emptiness. */
bool iter_min(Iter it, compare_fn cmp, void *out);

/* Write the maximum element to *out; returns 1 on success, 0 if empty.
 * out may be NULL to simply test for emptiness. */
bool iter_max(Iter it, compare_fn cmp, void *out);
