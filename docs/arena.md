# arena

Bump allocator backed by `mmap`'d memory blocks. Grows automatically when a
block is exhausted. A single `arena_free()` releases all memory at once.

**Header:** `src/arena/arena.h`

---

## Types

### `Allocator`

```c
typedef struct Allocator {
  alloc_fn   alloc;
  realloc_fn realloc;
  free_fn    free;   /* NULL for arena allocators */
  void      *ctx;
} Allocator;
```

Generic allocator interface passed to every container and iterator that needs
to allocate memory. Every collection stores the `Allocator` it was created
with, so a single program can freely mix allocators:

| Source | Use when |
|--------|----------|
| [`arena_allocator(a)`](#arena_allocator) | bulk lifetime — free everything in one call |
| [`scratch_allocator(&sc)`](#scratch_allocator) | temporary work inside a loop |
| [`sys_allocator()`](#sys_allocator) | per-collection lifetime via `_free()`, or when no arena is available |

Obtain one from [`arena_allocator()`](#arena_allocator),
[`scratch_allocator()`](#scratch_allocator), or
[`sys_allocator()`](#sys_allocator).

### `Scratch`

```c
typedef struct {
  Arena *arena;
  void  *saved_tail;
  size_t saved_pos;
} Scratch;
```

A checkpoint into an arena. Push to save the current position; pop to release
everything allocated since the push. Useful for short-lived temporary
allocations inside a loop.

---

## Functions

### `arena_create`

```c
Arena *arena_create(size_t capacity);
```

Create a new arena with an initial block of at least `capacity` bytes.

```c
Arena *a = arena_create(4096);
```

---

### `arena_alloc`

```c
void *arena_alloc(Arena *a, size_t size, size_t align);
```

Bump-allocate `size` bytes aligned to `align`. Triggers a new block if the
current one is full. Never returns NULL (aborts on OOM).

```c
int *buf = arena_alloc(a, 64 * sizeof(int), _Alignof(int));
```

---

### `arena_realloc`

```c
void *arena_realloc(Arena *a, void *ptr, size_t old_size,
                    size_t new_size, size_t align);
```

Grow or shrink an existing arena allocation. If `ptr` is the most recent bump
in the current block the in-place fast path is taken; otherwise a new
allocation is made and the contents are copied.

---

### `arena_reset`

```c
void arena_reset(Arena *a);
```

Reset the bump position to zero, reusing all previously allocated blocks. Does
not release memory back to the OS. All overflow blocks (if the arena grew
beyond its initial capacity) are retained.

---

### `arena_reset_hard`

```c
void arena_reset_hard(Arena *a);
```

Like `arena_reset`, but also `munmap`s every overflow block, keeping only
the original first block. Use this after a temporary allocation burst when
you want to return pages to the OS rather than retain them.

```c
// process a large batch that temporarily inflates the arena
arena_alloc(a, 4 * 1024 * 1024, 1);
// ...
arena_reset_hard(a); // shrinks back to the initial block
```

---

### `arena_free`

```c
void arena_free(Arena *a);
```

Release all memory owned by the arena back to the OS.

---

### `arena_allocator` {#arena_allocator}

```c
Allocator arena_allocator(Arena *arena);
```

Return an [`Allocator`](#allocator) that allocates from `arena`. Pass this to
any container or iterator that accepts an `Allocator`.

```c
Arena    *a   = arena_create(4096);
Allocator al  = arena_allocator(a);
Vec       v   = vec_create(sizeof(int), al);
```

---

### `arena_scratch_push`

```c
Scratch arena_scratch_push(Arena *arena);
```

Save the current bump position and return a [`Scratch`](#scratch) checkpoint.

---

### `arena_scratch_pop`

```c
void arena_scratch_pop(Scratch *scratch);
```

Restore the arena to the position saved in `scratch`, freeing all allocations
made after the push.

```c
Scratch sc = arena_scratch_push(a);
// ... temporary work ...
arena_scratch_pop(&sc);
// everything allocated above is gone
```

---

### `scratch_allocator` {#scratch_allocator}

```c
Allocator scratch_allocator(Scratch *scratch);
```

Return an [`Allocator`](#allocator) backed by a `Scratch`. Useful when you want
to pass an allocator to a helper but have the allocations tied to a scratch
lifetime rather than the whole arena.

---

### `sys_allocator` {#sys_allocator}

```c
Allocator sys_allocator(void);
```

Return an [`Allocator`](#allocator) backed by `malloc` / `realloc` / `free`.
Every allocation is independent and must be released by calling the
corresponding collection's `_free()` function.

Use `sys_allocator()` when:

- you want per-collection lifetime control rather than bulk-free via an arena
- you want to integrate with tooling that tracks `malloc`/`free` pairs (e.g.
  Valgrind, AddressSanitizer `malloc` interceptors)
- you are writing a short-lived utility or test where setting up an arena is
  unnecessary overhead

```c
// per-collection lifetime: caller is responsible for hashmap_free()
HashMap *m = hashmap_create(sizeof(int), sizeof(int),
                             hash_fnv1a, hash_eq_bytes,
                             sys_allocator());
hashmap_set(m, &k, &v);
// ...
hashmap_free(m); // releases all bucket/key/value memory
```

Mixing allocators is explicitly supported — different collections in the same
program can use different allocators:

```c
Arena *request_arena = arena_create(64 * 1024);

// results live for the whole request (freed with the arena)
Vec *results = vec_create(sizeof(Result), arena_allocator(request_arena));

// lookup table has its own lifetime, freed independently
HashMap *cache = hashmap_create(sizeof(int), sizeof(CacheEntry),
                                hash_fnv1a, hash_eq_bytes,
                                sys_allocator());

// ... handle request ...

arena_free(request_arena); // frees results
hashmap_free(cache);       // frees cache independently
```

---

### Query functions

```c
size_t arena_total_allocated(const Arena *a);
size_t arena_block_count(const Arena *a);
size_t arena_capacity(const Arena *a);
```

Introspection helpers for debugging and sizing decisions.

---

## Patterns

### Scratch for temporaries

```c
Arena  *a  = arena_create(1024 * 1024);
Scratch sc = arena_scratch_push(a);

// sort needs scratch space — freed at pop
Slice sorted = iter_sort(vec_iter(v), int_cmp, scratch_allocator(&sc));

arena_scratch_pop(&sc);
// sorted is now invalid; use it before popping
```

### Multiple arenas for different lifetimes

```c
Arena *long_lived  = arena_create(1 << 20);
Arena *per_request = arena_create(64 * 1024);

// process request using per_request allocator
arena_reset(per_request);  // reset between requests
```

### Mixing arenas and sys_allocator

```c
Arena *a = arena_create(4096);

// iterator chain: scratch-backed, freed at pop
Scratch sc = arena_scratch_push(a);
Slice sorted = iter_sort(vec_iter(v), int_cmp, scratch_allocator(&sc));
process(sorted);
arena_scratch_pop(&sc); // sorted memory released

// persistent lookup table: freed explicitly when the table is done
HashMap *index = hashmap_create(sizeof(char *), sizeof(int),
                                hash_fnv1a_str, hash_eq_str,
                                sys_allocator());
build_index(index, data);
// ... use index across multiple requests ...
hashmap_free(index);
```
