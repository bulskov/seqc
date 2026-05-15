# arena

`seqc` uses the companion **`arena_allocator`** library (bundled under
`libs/arena_allocator/`) for all memory management. The library is not part of
`seqc` itself — it can be used independently of any `seqc` collection.

See also: [arena_allocator README](../libs/arena_allocator/README.md)

**Headers:**

```c
#include "arena/allocator.h"      /* allocator_t, allocator_vtable_t */
#include "arena/growing_arena.h"  /* growing_arena_t */
#include "arena/scratch.h"        /* scratch_t */
```

---

## Types

### `allocator_t`

```c
typedef struct {
    const allocator_vtable_t *vt;
    void                     *ctx;
} allocator_t;
```

Generic allocator interface passed to every container and iterator that needs
to allocate memory. Every collection stores the `allocator_t` it was created
with, so a single program can freely mix allocators.

| Source                                   | Use when                                            |
| ---------------------------------------- | --------------------------------------------------- |
| `growing_arena_allocator(&a)`            | bulk lifetime — destroy everything in one call      |
| `scratch_allocator(&sc)`                 | temporary work inside a loop                        |
| custom vtable (e.g. malloc/free wrapper) | per-collection lifetime, or when no arena is needed |

---

### `growing_arena_t`

```c
typedef struct {
    /* opaque */
} growing_arena_t;
```

A bump allocator that grows by allocating new blocks from the OS as needed.
Declare on the stack and initialise with `growing_arena_init`.

---

### `scratch_t`

```c
typedef struct {
    /* opaque */
} scratch_t;
```

A checkpoint into a `growing_arena_t`. Begin to save the current position;
end to release everything allocated since the begin. Useful for short-lived
temporary allocations inside a loop.

---

## Functions

### `growing_arena_init`

```c
void growing_arena_init(growing_arena_t *a, size_t block_size);
```

Initialise an arena. `block_size` is the minimum size of each internal block;
the implementation rounds up to a page-aligned size.

```c
growing_arena_t arena;
growing_arena_init(&arena, 4096);
```

---

### `growing_arena_destroy`

```c
void growing_arena_destroy(growing_arena_t *a);
```

Release all memory owned by the arena back to the OS.

```c
growing_arena_destroy(&arena);
```

---

### `growing_arena_reset`

```c
void growing_arena_reset(growing_arena_t *a);
```

Reset the bump position to zero, reusing all committed blocks. Does not
release memory back to the OS — useful for reset-between-requests patterns.

---

### `growing_arena_allocator`

```c
allocator_t growing_arena_allocator(growing_arena_t *a);
```

Return an `allocator_t` backed by the arena. Pass this to any collection or
iterator that accepts an `allocator_t`.

```c
growing_arena_t arena;
growing_arena_init(&arena, 4096);
allocator_t al = growing_arena_allocator(&arena);

Vec *v = vec_create(sizeof(int), al);
```

---

### `growing_arena_scratch_begin`

```c
void growing_arena_scratch_begin(scratch_t *s, growing_arena_t *a);
```

Save the current bump position into `*s`.

---

### `scratch_end`

```c
void scratch_end(scratch_t *s);
```

Restore the arena to the position saved by `growing_arena_scratch_begin`,
releasing all allocations made since then.

---

### `scratch_allocator`

```c
allocator_t scratch_allocator(scratch_t *s);
```

Return an `allocator_t` backed by the scratch region.

---

## Usage patterns

### Bulk lifetime

```c
growing_arena_t arena;
growing_arena_init(&arena, 1 << 20);
allocator_t al = growing_arena_allocator(&arena);

Vec    *v = vec_create(sizeof(int), al);
HashMap *m = hashmap_create(sizeof(int), sizeof(int),
                            hash_fnv1a_int, hash_eq_int, al);
// all collections share the arena lifetime
growing_arena_destroy(&arena); // frees everything at once
```

### Scratch for temporary allocations

```c
growing_arena_t arena;
growing_arena_init(&arena, 4096);

scratch_t sc;
growing_arena_scratch_begin(&sc, &arena);
allocator_t tmp = scratch_allocator(&sc);

// sort needs temporary space
Slice sorted = iter_sort(vec_iter(v), int_cmp, tmp);
use(sorted);

scratch_end(&sc); // scratch memory released; sorted is now invalid
growing_arena_destroy(&arena);
```

### Reset between requests

```c
growing_arena_t arena;
growing_arena_init(&arena, 64 * 1024);
allocator_t al = growing_arena_allocator(&arena);

while (has_request()) {
    process_request(al);
    growing_arena_reset(&arena); // reclaim memory without OS roundtrip
}
growing_arena_destroy(&arena);
```
