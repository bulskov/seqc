# seqc

A C11 library providing composable, uniform abstractions over sequences of data.

## Idea

The core problem `seqc` addresses is simple: working with collections in C requires
writing the same iteration patterns over and over, tightly coupled to the specific
container. There is no standard way to compose operations like filtering, mapping, or
reducing across different data structures.

`seqc` solves this with a small set of orthogonal abstractions:

- **[Arena](docs/arena.md)** — a bump allocator from the companion `arena_allocator`
  library. Pass `growing_arena_allocator(&a)` to any collection for bulk lifetime
  management; a single `growing_arena_destroy(&a)` releases everything at once.
  Use `scratch_allocator(&sc)` for temporary work inside a loop, or a plain
  malloc/free allocator when you need per-collection lifetime control.
- **[slice_t](docs/slice.md)** — a fat pointer `{ptr, len, elem_size}`. The concrete, arena-owned result of
  materialising an iterator. Also the input to operations that need random access.
- **[iter_t](docs/iter.md)** — a lazy, forward iterator. Any source produces one. Adaptors transform
  an `iter_t` into another `iter_t`. Nothing runs until a terminal is called.
- **[vec_t](docs/vec.md)** — a growable array. Owns its buffer, produces an `iter_t` or a `slice_t` on demand.

The abstraction is intentionally `void *`-based. Type safety is the caller's responsibility.
There are no macros in the public interface.

```
vec_t / list_t / bstree_t / ...
      │
      ▼
    iter_t ──[filter]──[map]──[take]──[skip]──► iter_collect() ──► slice_t
      ▲                                              │
      └──────────── iter_from_slice ────────────────┘
```

## Design principles

- **Lazy by default** — adaptor chains allocate nothing until a terminal is called.
- **Caller owns memory** — allocators are passed in; the library never hides allocations.
- **Allocator-agnostic** — every collection takes an `allocator_t`. Arena, scratch, and
  `sys_allocator()` (malloc/free) are all first-class. Different collections in the
  same program can use different allocators.
- **No macros** — readability and debuggability over syntax sugar.
- **One job per type** — iterators transform, slices store, arenas own.
- **`void *` is the abstraction** — generics via element size, not code generation.

### Naming

Public types are lowercase `snake_case_t` matching their function prefix
(`vec_t` ↔ `vec_*`, `string_t` ↔ `string_*`), so a declaration and its
operations read as one module. Types are **not** otherwise `seqc_`-prefixed —
short names keep call sites readable. The sole exception is `seqc_stack_t`:
POSIX reserves `stack_t` (in `<signal.h>` for `sigaltstack`), so the stack type
carries the `seqc_` prefix to avoid the clash. Its functions stay `stack_*`.

## Building

Requires: a C11/C++ compiler (GCC or Clang), `cmake >= 3.20`, `ninja`.

```sh
./build.sh          # configure + build (debug)
./build.sh clean    # clean rebuild
./build.sh release  # release build
```

Or manually:

```sh
cmake --preset debug
cmake --build --preset debug
```

A `release` preset is also available.

## Testing

Tests use [Google Test](https://github.com/google/googletest), which is automatically
fetched by CMake the first time tests are built — no manual installation needed.

```sh
./test.sh                    # build + run all tests
ctest --test-dir build/debug --output-on-failure
```

To skip building tests: `cmake --preset debug -DBUILD_TESTING=OFF`.

## Scripts

| Script                      | Description                                                                                               |
| --------------------------- | --------------------------------------------------------------------------------------------------------- |
| `build.sh [clean] [preset]` | Configure and build. Pass `clean` to wipe the preset's build directory first. Preset defaults to `debug`. |
| `test.sh [debug\|asan]`     | Build and run the full test suite via `ctest`. `asan` builds with AddressSanitizer/UBSan/LeakSanitizer.   |
| `publish.sh`                | Build a release, then produce `dist/seqc-<version>/` (headers, lib, docs) and `seqc-<version>.zip`.       |

### Using as a dependency

**CMake FetchContent** (recommended):

```cmake
include(FetchContent)
FetchContent_Declare(seqc
    GIT_REPOSITORY https://github.com/bulskov/seqc
    GIT_TAG        master)
FetchContent_MakeAvailable(seqc)
target_link_libraries(myapp PRIVATE seqc)
```

Then include with:

```c
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/vec.h"
#include "seqc/iter.h"
```

`seqc` depends on the
[`arena_allocator`](https://github.com/bulskov/arena_allocation) library. seqc's
own CMake fetches it from source via `FetchContent`, so a FetchContent consumer
gets it transitively — `arena` is a public dependency of `seqc` and linking
`seqc` is enough:

```cmake
target_link_libraries(myapp PRIVATE seqc)
```

To pin a specific arena version or build offline, override the cache variables
seqc exposes:

```sh
cmake --preset debug \
    -DSEQC_ARENA_GIT_TAG=v1.1.1 \
    -DFETCHCONTENT_SOURCE_DIR_ARENA=/path/to/local/arena_allocation
```

**Manual vendoring**: copy `include/seqc/` and `src/` into your project, and
separately vendor [`arena_allocator`](https://github.com/bulskov/arena_allocation)
(its `include/` and `src/`). Add both include paths, compile all the `.c` files
alongside your own. The release archive produced by `publish.sh` bundles a
prebuilt `lib/libarena.a` and merged `include/` for this purpose.

## Modules

| Module    | Description                                           | Docs                               |
| --------- | ----------------------------------------------------- | ---------------------------------- |
| `arena`   | Bump allocator (external `arena_allocator` lib)       | [docs/arena.md](docs/arena.md)     |
| `slice`   | Non-owning contiguous view                            | [docs/slice.md](docs/slice.md)     |
| `iter`    | Lazy iterator pipeline — sources, adaptors, terminals | [docs/iter.md](docs/iter.md)       |
| `vec`     | Growable array                                        | [docs/vec.md](docs/vec.md)         |
| `stack`   | LIFO wrapper over vec_t                                 | [docs/stack.md](docs/stack.md)     |
| `queue`   | FIFO ring buffer                                      | [docs/queue.md](docs/queue.md)     |
| `ringbuf` | Double-ended circular buffer (deque)                  | [docs/ringbuf.md](docs/ringbuf.md) |
| `list`    | Singly-linked list                                    | [docs/list.md](docs/list.md)       |
| `dlist`   | Doubly-linked list                                    | [docs/dlist.md](docs/dlist.md)     |
| `set`     | Open-addressing hash set (Robin Hood)                 | [docs/set.md](docs/set.md)         |
| `hashmap` | Open-addressing hash map (Robin Hood)                 | [docs/hashmap.md](docs/hashmap.md) |
| `string`  | Bounded string + strbuf_t + iter sources         | [docs/string.md](docs/string.md)   |
| `string_io` | Optional stdio output for `string_t` (separate header)| [docs/string.md](docs/string.md)   |
| `bstree`  | Unbalanced binary search tree                         | [docs/bstree.md](docs/bstree.md)   |
| `avl`     | Self-balancing AVL tree                               | [docs/avl.md](docs/avl.md)         |
| `omap`    | Ordered map backed by AVL tree                        | [docs/omap.md](docs/omap.md)       |
| `pqueue`  | Binary min-heap priority queue                        | [docs/pqueue.md](docs/pqueue.md)   |

## Strings and stdio interop

`string_t` is a length-delimited view (`{ptr, len}`), not NUL-terminated. To
print one with the `printf` family without allocating or copying, use the
`STRING_FMT` / `STRING_ARG` macros — the standard `"%.*s"` idiom:

```c
printf("hello, " STRING_FMT "!\n", STRING_ARG(name));
```

For binary-safe output (exact byte count, tolerates embedded NULs), include
`seqc/string_io.h` and use `string_fwrite`, `string_print`, or
`string_println`. The header is separate so the core string type carries no
`<stdio.h>` dependency.

When an API genuinely requires a `const char *` (e.g. `fopen`, `getenv`),
`string_to_cstr_buf` copies into a caller-supplied stack buffer with no
allocator (or `string_to_cstr` for an allocator-owned copy):

```c
char path[256];
FILE *f = fopen(string_to_cstr_buf(p, path, sizeof path), "r");
```

## Quick example

```c
#include "arena/growing_arena.h"
#include "arena/scratch.h"
#include "seqc/vec.h"
#include "seqc/iter.h"

static int is_even(const void *elem, void *ctx) {
    return *(const int *)elem % 2 == 0;
}

static void double_it(const void *in, void *out, void *ctx) {
    *(int *)out = *(const int *)in * 2;
}

static int int_cmp(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

int main(void) {
    growing_arena_t arena;
    growing_arena_init(&arena, 4096);
    allocator_t al = growing_arena_allocator(&arena);

    vec_t *v = vec_create(sizeof(int), al);

    for (int i = 0; i < 10; i++)
        vec_push(v, &i);

    // filter evens, double them, sort, collect
    slice_t result = iter_sort(
                       iter_map(
                           iter_filter(vec_iter(v), is_even, NULL),
                           double_it, NULL, sizeof(int)),
                       int_cmp, al);

    // result == {0, 4, 8, 12, 16}
    for (size_t i = 0; i < result.len; i++)
        printf("%d\n", *(int *)slice_get(result, i));

    growing_arena_destroy(&arena);
}
```

## Iterator overview

### Sources

| Function                                                                 | Yields                                              | Docs                       |
| ------------------------------------------------------------------------ | --------------------------------------------------- | -------------------------- |
| `iter_from_slice(s, al)`                                                 | slice_t elements                                      | [iter](docs/iter.md)       |
| `iter_from_slice_rev(s, al)`                                             | slice_t elements in reverse                           | [iter](docs/iter.md)       |
| `iter_generate(fn, ctx, elem_size, al)`                                  | Stateful generator; stops when `fn` returns `false` | [iter](docs/iter.md)       |
| `iter_range(start, end, step, al)`                                       | `long long` integer range                           | [iter](docs/iter.md)       |
| `vec_iter(&v)` / `vec_iter_rev(&v)`                                      | vec_t elements                                        | [vec](docs/vec.md)         |
| `stack_iter(&s)`                                                         | stack elements bottom→top                           | [stack](docs/stack.md)     |
| `queue_iter(&q)`                                                         | queue elements front→back                           | [queue](docs/queue.md)     |
| `ringbuf_iter(r)` / `ringbuf_iter_rev(r)`                                | ringbuf_t elements front→back / reverse               | [ringbuf](docs/ringbuf.md) |
| `list_iter(&l)`                                                          | list elements front→back                            | [list](docs/list.md)       |
| `dlist_iter(&l)` / `dlist_iter_rev(&l)`                                  | dlist_t forward / reverse                             | [dlist](docs/dlist.md)     |
| `set_iter(&s)` / `set_iter_rev(&s)`                                      | set elements (unordered)                            | [set](docs/set.md)         |
| `hashmap_iter(m)` / `hashmap_iter_rev(m)`                                | `map_entry_t` pairs (`hashmap_entry_t` alias)             | [hashmap](docs/hashmap.md) |
| `string_chars(s, al)` / `string_chars_rev(s, al)`                        | `char` values                                       | [string](docs/string.md)   |
| `string_split_substr(s, delim, al)` / `string_split_any(s, set, al)`     | `string_t` tokens                                     | [string](docs/string.md)   |
| `bstree_iter(t)` / `bstree_iter_rev(t)` / `bstree_iter_range(t, lo, hi)` | BST elements                                        | [bstree](docs/bstree.md)   |
| `avl_iter(&t)` / `avl_iter_rev(&t)` / `avl_iter_range(&t, lo, hi)`       | AVL elements                                        | [avl](docs/avl.md)         |
| `omap_iter(&m)` / `omap_iter_rev(&m)` / `omap_iter_range(&m, lo, hi)`    | `map_entry_t` pairs (`omap_entry_t` alias)                | [omap](docs/omap.md)       |

### Adaptors (lazy)

| Function                               | Description                                         | Docs                 |
| -------------------------------------- | --------------------------------------------------- | -------------------- |
| `iter_filter(it, pred, ctx)`           | Keep matching elements                              | [iter](docs/iter.md) |
| `iter_map(it, fn, ctx, out_size)`      | Transform each element                              | [iter](docs/iter.md) |
| `iter_take(it, n)`                     | Yield at most n elements                            | [iter](docs/iter.md) |
| `iter_take_while(it, pred, ctx)`       | Yield while pred holds, stop at first miss          | [iter](docs/iter.md) |
| `iter_skip(it, n)`                     | Skip first n elements                               | [iter](docs/iter.md) |
| `iter_skip_while(it, pred, ctx)`       | Skip while pred holds, yield the rest               | [iter](docs/iter.md) |
| `iter_chain(a, b)`                     | Concatenate two iterators                           | [iter](docs/iter.md) |
| `iter_zip(a, b)`                       | Interleave element pairs                            | [iter](docs/iter.md) |
| `iter_enumerate(it)`                   | Pair each element with its index                    | [iter](docs/iter.md) |
| `iter_window(it, n)`                   | Sliding window of size n (yields slice_t)             | [iter](docs/iter.md) |
| `iter_chunks(it, n)`                   | Non-overlapping chunks of size n (yields slice_t)     | [iter](docs/iter.md) |
| `iter_flat_map(it, fn, ctx, out_size)` | Expand each element into a sub-iterator             | [iter](docs/iter.md) |
| `iter_peekable(it)`                    | Wrap so `iter_peek()` can inspect without consuming | [iter](docs/iter.md) |
| `iter_dedup(it, cmp)`                  | Skip consecutive equal elements                     | [iter](docs/iter.md) |

### Terminals (consume)

| Function                        | Returns              | Docs                 |
| ------------------------------- | -------------------- | -------------------- |
| `iter_collect(it, al)`          | Arena-owned `slice_t`  | [iter](docs/iter.md) |
| `iter_count(it)`                | `size_t` count       | [iter](docs/iter.md) |
| `iter_foreach(it, fn, ctx)`     | — (side-effects)     | [iter](docs/iter.md) |
| `iter_reduce(it, acc, fn, ctx)` | — (folds into acc)   | [iter](docs/iter.md) |
| `iter_sort(it, cmp, al)`        | Sorted `slice_t`       | [iter](docs/iter.md) |
| `iter_find(it, pred, ctx, out)` | `bool` + first match | [iter](docs/iter.md) |
| `iter_any(it, pred, ctx)`       | `true` if any match  | [iter](docs/iter.md) |
| `iter_all(it, pred, ctx)`       | `true` if all match  | [iter](docs/iter.md) |
| `iter_min(it, cmp, out)`        | `bool` + minimum     | [iter](docs/iter.md) |
| `iter_max(it, cmp, out)`        | `bool` + maximum     | [iter](docs/iter.md) |

## Test coverage

Measured with `llvm-cov` (clang 18, instrumented build).
The test suite uses [Google Test](https://github.com/google/googletest).

| Module    |   Lines | Functions | Branches |
| --------- | ------: | --------: | -------: |
| `slice`   |     95% |      100% |      69% |
| `iter`    |     90% |      100% |      79% |
| `vec`     |     88% |      100% |      65% |
| `stack`   |     92% |      100% |      62% |
| `queue`   |     95% |      100% |      68% |
| `list`    |     95% |      100% |      74% |
| `dlist`   |     92% |      100% |      65% |
| `set`     |     95% |      100% |      78% |
| `hashmap` |     90% |      100% |      74% |
| `string`  |     94% |      100% |      72% |
| `bstree`  |     92% |      100% |      73% |
| `avl`     |     93% |      100% |      76% |
| `omap`    |     88% |       97% |      67% |
| `pqueue`  |     98% |      100% |      80% |
| **Total** | **92%** |  **100%** |  **73%** |

The remaining branch gaps are defensive NULL-pointer guards and `align`
parameters that are always `_Alignof(max_align_t)` in practice.

## Known gaps / roadmap
