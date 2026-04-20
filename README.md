# seqc

A C11 library providing composable, uniform abstractions over sequences of data.

## Idea

The core problem `seqc` addresses is simple: working with collections in C requires
writing the same iteration patterns over and over, tightly coupled to the specific
container. There is no standard way to compose operations like filtering, mapping, or
reducing across different data structures.

`seqc` solves this with a small set of orthogonal abstractions:

- **`Iter`** — a lazy, forward iterator. Any source (array, vec, list, ...) produces one.
  Adaptors transform an `Iter` into another `Iter`. Nothing runs until a terminal is called.
- **`Slice`** — a fat pointer `{ptr, len, elem_size}`. The concrete, arena-owned result of
  materialising an iterator. Also the input to operations that need random access (sort, etc.).
- **`Arena`** — a bump allocator. All heap ownership lives here. Callers decide lifetime;
  a single `arena_free()` releases everything at once.
- **`Vec`** — a growable array. Owns its buffer, produces an `Iter` or a `Slice` on demand.

The abstraction is intentionally `void *`-based. Type safety is the caller's responsibility.
There are no macros in the public interface.

```
Vec / array / ...
      │
      ▼
    Iter ──[filter]──[map]──[take]──[skip]──► iter_collect(arena) ──► Slice
      ▲                                              │
      └──────────── iter_from_slice ─────────────────┘
```

## Design principles

- **Lazy by default** — adaptor chains allocate nothing until a terminal is called.
- **Caller owns memory** — arenas are passed in; the library never hides allocations.
- **No macros** — readability and debuggability over syntax sugar.
- **One job per type** — iterators transform, slices store, arenas own.
- **`void *` is the abstraction** — generics via element size, not code generation.

## Building

Requires: `clang`, `cmake >= 3.20`, `ninja`, `criterion` (test framework).

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

A `release` preset is also available.

## What is implemented

### `arena`
Bump allocator backed by a single heap buffer. Grows automatically if capacity is exceeded.

| Function | Description |
|---|---|
| `arena_create(capacity)` | Allocate a new arena with an initial capacity |
| `arena_alloc(arena, size, align)` | Bump-allocate `size` bytes with the given alignment |
| `arena_reset(arena)` | Reset position to zero, reusing the buffer |
| `arena_free(arena)` | Release all memory |

### `slice`
A non-owning view into a contiguous block of elements.

| Function | Description |
|---|---|
| `slice_get(slice, i)` | Return a pointer to element `i` |

### `iter`
The core protocol. Every source and adaptor is an `Iter`.

**Sources**

| Function | Description |
|---|---|
| `iter_from_slice(slice)` | Iterate over a slice |

**Adaptors** (lazy — return a new `Iter`)

| Function | Description |
|---|---|
| `iter_filter(it, pred, ctx)` | Keep elements where `pred` returns non-zero |
| `iter_map(it, fn, ctx, out_size)` | Transform each element to a (possibly different) type |
| `iter_take(it, n)` | Yield at most `n` elements |
| `iter_skip(it, n)` | Skip the first `n` elements |

**Terminals** (consume the iterator)

| Function | Description |
|---|---|
| `iter_collect(it, arena)` | Materialise into an arena-owned `Slice` |
| `iter_count(it)` | Count remaining elements |
| `iter_foreach(it, fn, ctx)` | Call `fn` for each element |
| `iter_reduce(it, acc, fn, ctx)` | Fold elements into an accumulator |

### `vec`
A growable, heap-owned array.

| Function | Description |
|---|---|
| `vec_create(elem_size)` | Create an empty vec |
| `vec_push(vec, elem)` | Append a copy of `elem` |
| `vec_get(vec, i)` | Pointer to element `i` |
| `vec_as_slice(vec)` | Non-owning `Slice` view |
| `vec_iter(vec)` | Create an `Iter` over the vec |
| `vec_free(vec)` | Release the buffer |

## Roadmap

### Next steps

- **`slice_sort(slice, arena, cmp)`** — sort into a new arena-owned slice; enables order-by
- **`iter_zip(it_a, it_b)`** — pair elements from two iterators
- **`iter_flat_map`** — each element produces a sub-iterator, results are flattened
- **`iter_first` / `iter_any` / `iter_all`** — short-circuiting terminals

### Data structures

- **`list`** — singly-linked list, arena-backed (no per-node `malloc`), exposes `Iter`
- **`map`** — open-addressing hash map, exposes a key-value pair `Iter`

### Later

- **Batch / chunk iterator** — `next_chunk(buf, max, written)` to amortise function-pointer
  overhead and enable SIMD in adaptors
- **`iter_from_file` / `iter_lines`** — I/O sources feeding the same pipeline
- **Cross-platform support** — currently Linux/clang; MSVC and GCC compatibility
- **`slice_sort` in-place variant** — for cases where a copy is unnecessary
