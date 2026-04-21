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

/* --- Adaptors (take ownership of source) -------------------------------- */

Iter iter_filter(Iter source, pred_fn pred, void *ctx);

Iter iter_map(Iter source, map_fn map, void *ctx, size_t out_elem_size);

Iter iter_take(Iter source, size_t n);
Iter iter_skip(Iter source, size_t n);

/* --- Terminals (consume and drop the iterator) -------------------------- */

Slice iter_collect(Iter it);
size_t iter_count(Iter it);
void iter_foreach(Iter it, visitor_fn visit, void *ctx);
void iter_reduce(Iter it, void *acc, combine_fn combine, void *ctx);
