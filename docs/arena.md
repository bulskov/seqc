# arena

`seqc` uses the companion **`arena_allocator`** library (fetched from source by
CMake via `FetchContent`) for all memory management. The library is not part of
`seqc` itself — it can be used independently of any `seqc` collection.

See also: [arena_allocator README](https://github.com/bulskov/arena_allocation)

**Headers:**

```c
#include "arena/allocator.h"      /* allocator_t, allocator_vtable_t, arena_stats_t */
#include "arena/growing_arena.h"  /* growing_arena_t  — unbounded, grows on demand */
#include "arena/fixed_arena.h"    /* fixed_arena_t    — bounded, fully committed   */
#include "arena/virtual_arena.h"  /* virtual_arena_t  — bounded, committed on demand */
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
| `virtual_arena_allocator(&a)`            | bulk lifetime **with a hard memory ceiling**        |
| `fixed_arena_allocator(&a)`              | a caller-owned buffer, or a small fully-committed cap |
| `scratch_allocator(&sc)`                 | temporary work inside a loop                        |
| custom vtable (e.g. malloc/free wrapper) | per-collection lifetime, or when no arena is needed |

Because every collection stores its `allocator_t` **by value**, `seqc` itself is
allocator-agnostic: imposing a memory ceiling is entirely a matter of which
arena you construct at start-up. No `seqc` API takes or needs a limit
parameter. See [Bounded allocation](#bounded-allocation---max-mem).

---

### `growing_arena_t`

```c
typedef struct {
    /* opaque */
} growing_arena_t;
```

A bump allocator that grows by allocating new blocks from the OS as needed.
Declare on the stack and initialise with `growing_arena_init`.

**Unbounded.** It will keep requesting blocks until the OS refuses. If you need
an upper limit, use `virtual_arena_t` or `fixed_arena_t` instead.

---

### `fixed_arena_t`

```c
typedef struct {
    /* opaque */
} fixed_arena_t;
```

A bump allocator over one contiguous region of a **fixed size**, fully
committed up front. Once the region is exhausted, `mem_alloc` returns `NULL` —
it never grows. Either attach it to a buffer you own (`fixed_arena_init`) or
let it map its own (`fixed_arena_create`).

---

### `virtual_arena_t`

```c
typedef struct {
    /* opaque */
} virtual_arena_t;
```

A bump allocator over a **reserved virtual address range**. The full range is
reserved at init, but no physical pages are committed until allocations demand
them, in `commit_chunk` increments. Allocations past the reservation return
`NULL`.

This is the arena to reach for when you want growing-arena behaviour *and* a
hard ceiling: address space is cheap, and you only pay for resident pages while
they are in use.

---

### `arena_stats_t`

```c
typedef struct {
    size_t used;      /* bytes currently handed out          */
    size_t capacity;  /* total backing capacity              */
    size_t committed; /* physical pages committed            */
    size_t reserved;  /* reserved virtual address range      */
} arena_stats_t;
```

Snapshot returned by each arena's `*_stats()` function. For `growing_arena_t`
and `fixed_arena_t` the last three fields coincide; for `virtual_arena_t`,
`committed` tracks resident memory while `reserved` stays at the ceiling.

---

### `scratch_t`

```c
typedef struct {
    /* opaque */
} scratch_t;
```

A checkpoint into any bump arena — `growing_arena_t`, `fixed_arena_t`, or
`virtual_arena_t`. Begin to save the current position; end to release
everything allocated since the begin. Useful for short-lived temporary
allocations inside a loop.

Each arena provides its own begin (`growing_arena_scratch_begin`,
`fixed_arena_scratch_begin`, `virtual_arena_scratch_begin`); `scratch_end` and
`scratch_allocator` are common to all three. Scratches on the same arena must
be strictly LIFO — innermost ended first.

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

Rewind to an empty arena, keeping the head block for reuse and releasing every
other block back to the OS. Useful for reset-between-requests patterns, where
the head block absorbs the steady-state working set without a syscall.

---

### `growing_arena_reset_full`

```c
void growing_arena_reset_full(growing_arena_t *a);
```

Rewind and release **every** block, including the head. The arena stays usable
and maps a fresh block on the next allocation. Use after a spike, to hand the
pages back.

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

vec_t *v = vec_create(sizeof(int), al);
```

---

### `growing_arena_scratch_begin`

```c
void growing_arena_scratch_begin(scratch_t *s, growing_arena_t *a);
```

Save the current bump position into `*s`.

---

### `growing_arena_stats`

```c
arena_stats_t growing_arena_stats(const growing_arena_t *a);
```

Snapshot of bytes handed out and total block capacity. Walks the block chain,
so it is O(blocks) rather than O(1).

---

### `fixed_arena_init` / `fixed_arena_create`

```c
void fixed_arena_init(fixed_arena_t *a, void *buf, size_t size);
int  fixed_arena_create(fixed_arena_t *a, size_t size);   /* 0 ok, -1 failed */
```

`fixed_arena_init` attaches the arena to a buffer you own — static, stack, or
heap — and never frees it. `fixed_arena_create` maps `size` bytes itself and
takes ownership; it returns `-1` if the mapping fails.

```c
static uint8_t buf[64 * 1024];
fixed_arena_t fa;
fixed_arena_init(&fa, buf, sizeof buf);   /* zero syscalls, zero heap */
```

---

### `fixed_arena_destroy` / `fixed_arena_reset`

```c
void fixed_arena_destroy(fixed_arena_t *a);
void fixed_arena_reset(fixed_arena_t *a);
```

`fixed_arena_destroy` unmaps the region if the arena owns it, and is a no-op
for a caller-supplied buffer — safe to call either way. `fixed_arena_reset`
rewinds the bump position, leaving the region itself untouched.

---

### `fixed_arena_allocator` / `fixed_arena_allocator_new`

```c
allocator_t fixed_arena_allocator(fixed_arena_t *a);
allocator_t fixed_arena_allocator_new(fixed_arena_t *a, size_t size);
```

`fixed_arena_allocator_new` combines `fixed_arena_create` and
`fixed_arena_allocator`. On failure it returns `ALLOCATOR_NULL`, a valid
allocator whose every allocation returns `NULL` — so a missed error check
surfaces as an allocation failure rather than a crash.

---

### `virtual_arena_init`

```c
int virtual_arena_init(virtual_arena_t *a, size_t reserved_size,
                       size_t commit_chunk);   /* 0 ok, -1 failed */
```

Reserve `reserved_size` bytes of address space without committing any physical
pages. `commit_chunk` is the granularity in which pages are committed as
allocations arrive, rounded up to the page size — larger chunks mean fewer
syscalls, finer chunks mean tighter resident memory.

`reserved_size` is the hard ceiling: an allocation that would push past it
returns `NULL`.

---

### `virtual_arena_destroy` / `virtual_arena_reset`

```c
void virtual_arena_destroy(virtual_arena_t *a);
void virtual_arena_reset(virtual_arena_t *a);
```

`virtual_arena_reset` decommits every page — handing physical memory back to
the OS — while keeping the address reservation, so the ceiling still applies
and no re-reservation is needed. `virtual_arena_destroy` releases the range
entirely.

---

### `virtual_arena_allocator` / `virtual_arena_allocator_new`

```c
allocator_t virtual_arena_allocator(virtual_arena_t *a);
allocator_t virtual_arena_allocator_new(virtual_arena_t *a,
                                        size_t reserved_size,
                                        size_t commit_chunk);
```

As with the fixed arena, the `_new` form combines init and allocator, returning
`ALLOCATOR_NULL` if the reservation fails.

---

### `fixed_arena_stats` / `virtual_arena_stats`

```c
arena_stats_t fixed_arena_stats(const fixed_arena_t *a);
arena_stats_t virtual_arena_stats(const virtual_arena_t *a);
```

O(1) snapshots. For `virtual_arena_t`, `used` is the bump position, `committed`
is resident memory, and `reserved` is the configured ceiling — enough to report
"used X of Y" for a memory-limit flag.

---

### `fixed_arena_scratch_begin` / `virtual_arena_scratch_begin`

```c
void fixed_arena_scratch_begin(scratch_t *s, fixed_arena_t *a);
void virtual_arena_scratch_begin(scratch_t *s, virtual_arena_t *a);
```

The bounded-arena counterparts of `growing_arena_scratch_begin`. Pair with
`scratch_end` as usual.

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

vec_t    *v = vec_create(sizeof(int), al);
hashmap_t *m = hashmap_create(sizeof(int), sizeof(int),
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
slice_t sorted = iter_sort(vec_iter(v), int_cmp, tmp);
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

### Bounded allocation (`--max-mem`)

To give a program a hard memory ceiling, choose a bounded arena at start-up.
Nothing else changes: the collections receive an `allocator_t` exactly as
before, and when the ceiling is hit the arena returns `NULL`, which every
`seqc` entry point reports as `SEQC_OOM` (or a `NULL` handle from a `*_create`).

```c
/* --max-mem <bytes>, 0 meaning "no limit" */
static growing_arena_t g_growing;
static virtual_arena_t g_virtual;

allocator_t make_allocator(size_t max_mem)
{
    if (max_mem == 0) {
        growing_arena_init(&g_growing, 1 << 20);
        return growing_arena_allocator(&g_growing);
    }
    /* Reserve the ceiling; commit 64 KiB at a time as it is actually used. */
    return virtual_arena_allocator_new(&g_virtual, max_mem, 64 * 1024);
}
```

Both branches yield an ordinary `allocator_t`, so every call site downstream is
identical:

```c
allocator_t al = make_allocator(opts.max_mem);

vec_t *v = vec_create(sizeof(record_t), al);
if (!v)
    return oom("--max-mem exceeded during startup");

for (size_t i = 0; i < n; i++) {
    if (vec_push(v, &records[i]) == SEQC_OOM)
        return oom("--max-mem exceeded");   /* ceiling reached, not a crash */
}
```

`virtual_arena_t` is the right default for a memory limit: it reserves address
space rather than physical memory, so a generous ceiling costs nothing until
the program actually grows into it. Reach for `fixed_arena_t` only when the
region must be committed up front, or when it should live in a buffer you
already own.

To report progress against the limit:

```c
arena_stats_t st = virtual_arena_stats(&g_virtual);
fprintf(stderr, "memory: %zu KiB used, %zu KiB resident, limit %zu KiB\n",
        st.used / 1024, st.committed / 1024, st.reserved / 1024);
```

> **Sizing.** A bump arena never reclaims memory on `realloc`, so the ceiling
> bounds *total bytes ever handed out*, not live data. A `vec_t` that grows
> geometrically leaves each superseded buffer behind, so it will hit a limit at
> roughly half of it. Size `--max-mem` for the allocation traffic, not the
> working set, or call `growing_arena_reset` / `virtual_arena_reset` between
> phases to reclaim.

> **Note.** `growing_arena_t` has no ceiling of its own — it keeps mapping new
> blocks until the OS refuses. Building a memory limit on top of it (for
> example by wrapping the allocator) is unnecessary: `virtual_arena_t` already
> provides growing behaviour with a bound.
