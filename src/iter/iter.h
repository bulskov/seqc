#pragma once

#include <stddef.h>

#include "slice/slice.h"

/* Forward declaration — include arena/arena.h to use iter_collect */
typedef struct Arena Arena;

typedef struct Iter Iter;

struct Iter {
  int (*next)(Iter *it, void *out); /* writes elem_size bytes; returns 1 or 0 */
  void (*drop)(Iter *it); /* frees internal state; NULL is valid     */
  void *state;
  size_t elem_size;
};

/* Inline drop — call to release resources without a terminal */
static inline void iter_drop(Iter *it) {
  if (it && it->drop)
    it->drop(it);
}

/* --- Sources ------------------------------------------------------------ */

Iter iter_from_slice(Slice s);

/* --- Adaptors (take ownership of source) -------------------------------- */

Iter iter_filter(Iter source, int (*pred)(const void *elem, void *ctx),
                 void *ctx);

Iter iter_map(Iter source, void (*map)(const void *in, void *out, void *ctx),
              void *ctx, size_t out_elem_size);

Iter iter_take(Iter source, size_t n);
Iter iter_skip(Iter source, size_t n);

/* --- Terminals (consume and drop the iterator) -------------------------- */

Slice iter_collect(Iter it, Arena *a);
size_t iter_count(Iter it);
void iter_foreach(Iter it, void (*fn)(const void *elem, void *ctx), void *ctx);
void iter_reduce(Iter it, void *acc,
                 void (*combine)(void *acc, const void *elem, void *ctx),
                 void *ctx);
