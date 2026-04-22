#pragma once

#include <stddef.h>

#include "arena/arena.h"
#include "slice/slice.h"

/* Forward declaration — include arena/arena.h to use iter_collect */

typedef struct Iter Iter;

typedef int (*pred_fn)(const void *elem, void *ctx);
typedef void (*map_fn)(const void *in, void *out, void *ctx);
typedef void (*combine_fn)(void *acc, const void *elem, void *ctx);
typedef void (*visitor_fn)(const void *elem, void *ctx);
typedef int (*compare_fn)(const void *a, const void *b);

struct Iter {
  int (*next)(Iter *it, void *out); /* writes elem_size bytes; returns 1 or 0 */
  void (*drop)(Iter *it); /* frees internal state; NULL is valid     */
  void *state;
  size_t elem_size;
  Allocator allocator; /* used by adaptors that need to allocate state */
};

/* Inline drop — call to release resources without a terminal */
static inline void iter_drop(Iter *it) {
  if (it && it->drop)
    it->drop(it);
}

/* --- Sources ------------------------------------------------------------ */

Iter iter_from_slice(Slice s, Allocator allocator);
Iter iter_from_slice_rev(Slice s, Allocator allocator);

/* --- Adaptors (take ownership of source) -------------------------------- */

Iter iter_filter(Iter source, pred_fn pred, void *ctx);

Iter iter_map(Iter source, map_fn map, void *ctx, size_t out_elem_size);

Iter iter_take(Iter source, size_t n);
Iter iter_skip(Iter source, size_t n);

/* Yields all of a then all of b; elem_size must match */
Iter iter_chain(Iter a, Iter b);

/* Yields flat [a_elem | b_elem] pairs; stops when either source ends.
 * elem_size of result == a.elem_size + b.elem_size */
Iter iter_zip(Iter a, Iter b);

/* Pairs each element with its 0-based index.
 * Yields EnumEntry {index, elem} where elem points into an internal buffer.
 * Do not store elem across calls. */
typedef struct {
  size_t index;
  void *elem; /* pointer into internal buffer — valid until next call */
} EnumEntry;

Iter iter_enumerate(Iter source);

/* --- Terminals (consume and drop the iterator) -------------------------- */

Slice iter_collect(Iter it);
size_t iter_count(Iter it);
void iter_foreach(Iter it, visitor_fn visit, void *ctx);
void iter_reduce(Iter it, void *acc, combine_fn combine, void *ctx);

/* Collects and sorts in-place using cmp; returns sorted Slice */
Slice iter_sort(Iter it, compare_fn cmp);

/* Returns 1 and writes first match to *out (may be NULL) if found */
int iter_find(Iter it, pred_fn pred, void *ctx, void *out);

/* Returns 1 if any element satisfies pred */
int iter_any(Iter it, pred_fn pred, void *ctx);

/* Returns 1 if all elements satisfy pred (vacuously true for empty) */
int iter_all(Iter it, pred_fn pred, void *ctx);
