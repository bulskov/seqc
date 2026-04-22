# seqc

A C11 library providing composable, uniform abstractions over sequences of data.

## Idea

The core problem `seqc` addresses is simple: working with collections in C requires
writing the same iteration patterns over and over, tightly coupled to the specific
container. There is no standard way to compose operations like filtering, mapping, or
reducing across different data structures.

`seqc` solves this with a small set of orthogonal abstractions:

- **[Arena](docs/arena.md)** — a bump allocator. Pass `arena_allocator(a)` to any
  collection for bulk lifetime management; a single `arena_free()` releases everything
  at once. Use `scratch_allocator(&sc)` for temporary work inside a loop, or
  `sys_allocator()` when you need per-collection `malloc`/`free` lifetime control.
- **[Slice](docs/slice.md)** — a fat pointer `{ptr, len, elem_size}`. The concrete, arena-owned result of
  materialising an iterator. Also the input to operations that need random access.
- **[Iter](docs/iter.md)** — a lazy, forward iterator. Any source produces one. Adaptors transform
  an `Iter` into another `Iter`. Nothing runs until a terminal is called.
- **[Vec](docs/vec.md)** — a growable array. Owns its buffer, produces an `Iter` or a `Slice` on demand.

The abstraction is intentionally `void *`-based. Type safety is the caller's responsibility.
There are no macros in the public interface.

```
Vec / List / BTree / ...
      │
      ▼
    Iter ──[filter]──[map]──[take]──[skip]──► iter_collect() ──► Slice
      ▲                                              │
      └──────────── iter_from_slice ────────────────┘
```

## Design principles

- **Lazy by default** — adaptor chains allocate nothing until a terminal is called.
- **Caller owns memory** — allocators are passed in; the library never hides allocations.
- **Allocator-agnostic** — every collection takes an `Allocator`. Arena, scratch, and
  `sys_allocator()` (malloc/free) are all first-class. Different collections in the
  same program can use different allocators.
- **No macros** — readability and debuggability over syntax sugar.
- **One job per type** — iterators transform, slices store, arenas own.
- **`void *` is the abstraction** — generics via element size, not code generation.

## Building

Requires: `clang`, `cmake >= 3.20`, `ninja`.
Optional: `criterion` for building the test suite.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

If Criterion is not installed, the library still configures and builds, but test targets are skipped.
To silence the warning explicitly, configure with `-DBUILD_TESTING=OFF`.

A `release` preset is also available.

## Modules

| Module    | Description                                           | Docs                               |
| --------- | ----------------------------------------------------- | ---------------------------------- |
| `arena`   | Bump allocator with scratch checkpoints               | [docs/arena.md](docs/arena.md)     |
| `slice`   | Non-owning contiguous view                            | [docs/slice.md](docs/slice.md)     |
| `iter`    | Lazy iterator pipeline — sources, adaptors, terminals | [docs/iter.md](docs/iter.md)       |
| `vec`     | Growable array                                        | [docs/vec.md](docs/vec.md)         |
| `stack`   | LIFO wrapper over Vec                                 | [docs/stack.md](docs/stack.md)     |
| `queue`   | FIFO ring buffer                                      | [docs/queue.md](docs/queue.md)     |
| `list`    | Singly-linked list                                    | [docs/list.md](docs/list.md)       |
| `dlist`   | Doubly-linked list                                    | [docs/dlist.md](docs/dlist.md)     |
| `set`     | Open-addressing hash set (Robin Hood)                 | [docs/set.md](docs/set.md)         |
| `hashmap` | Open-addressing hash map (Robin Hood)                 | [docs/hashmap.md](docs/hashmap.md) |
| `string`  | Bounded string + StringBuilder + iter sources         | [docs/string.md](docs/string.md)   |
| `btree`   | Unbalanced binary search tree                         | [docs/btree.md](docs/btree.md)     |
| `avl`     | Self-balancing AVL tree                               | [docs/avl.md](docs/avl.md)         |
| `omap`    | Ordered map backed by AVL tree                        | [docs/omap.md](docs/omap.md)       |
| `pqueue`  | Binary min-heap priority queue                        | [docs/pqueue.md](docs/pqueue.md)   |

## Quick example

```c
#include "arena/arena.h"
#include "vec/vec.h"
#include "iter/iter.h"

static int is_even(const void *elem, void *ctx) {
    return *(const int *)elem % 2 == 0;
}

static void double_it(const void *in, void *out, void *ctx) {
    *(int *)out = *(const int *)in * 2;
}

static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

int main(void) {
    Arena *a = arena_create(4096);
    Vec    v = vec_create(sizeof(int), arena_allocator(a));

    for (int i = 0; i < 10; i++)
        vec_push(&v, &i);

    // filter evens, double them, sort, collect
    Slice result = iter_sort(
                       iter_map(
                           iter_filter(vec_iter(&v), is_even, NULL),
                           double_it, NULL, sizeof(int)),
                       int_cmp, arena_allocator(a));

    // result == {0, 4, 8, 12, 16}
    for (size_t i = 0; i < result.len; i++)
        printf("%d\n", *(int *)slice_get(result, i));

    arena_free(a);
}
```

## Iterator overview

### Sources

| Function                                                                 | Yields                                              | Docs                       |
| ------------------------------------------------------------------------ | --------------------------------------------------- | -------------------------- |
| `iter_from_slice(s, al)`                                                 | Slice elements                                      | [iter](docs/iter.md)       |
| `iter_from_slice_rev(s, al)`                                             | Slice elements in reverse                           | [iter](docs/iter.md)       |
| `iter_generate(fn, ctx, elem_size, al)`                                  | Stateful generator; stops when `fn` returns `false` | [iter](docs/iter.md)       |
| `iter_range(start, end, step, al)`                                       | `long long` integer range                           | [iter](docs/iter.md)       |
| `vec_iter(&v)` / `vec_iter_rev(&v)`                                      | Vec elements                                        | [vec](docs/vec.md)         |
| `stack_iter(&s)`                                                         | Stack elements bottom→top                           | [stack](docs/stack.md)     |
| `queue_iter(&q)`                                                         | Queue elements front→back                           | [queue](docs/queue.md)     |
| `list_iter(&l)`                                                          | List elements front→back                            | [list](docs/list.md)       |
| `dlist_iter(&l)` / `dlist_iter_reverse(&l)`                              | DList forward / reverse                             | [dlist](docs/dlist.md)     |
| `set_iter(&s)` / `set_iter_rev(&s)`                                      | Set elements (unordered)                            | [set](docs/set.md)         |
| `iter_from_hashmap(&m)`                                                  | `MapEntry` pairs (`HashMapEntry` alias)             | [hashmap](docs/hashmap.md) |
| `string_chars(s, al)` / `string_chars_rev(s, al)`                        | `char` values                                       | [string](docs/string.md)   |
| `string_split(s, delim, al)`                                             | `String` tokens                                     | [string](docs/string.md)   |
| `btree_iter(&t)` / `btree_iter_rev(&t)` / `btree_iter_range(&t, lo, hi)` | BST elements                                        | [btree](docs/btree.md)     |
| `avl_iter(&t)` / `avl_iter_rev(&t)` / `avl_iter_range(&t, lo, hi)`       | AVL elements                                        | [avl](docs/avl.md)         |
| `omap_iter(&m)` / `omap_iter_rev(&m)` / `omap_iter_range(&m, lo, hi)`    | `MapEntry` pairs (`OMapEntry` alias)                | [omap](docs/omap.md)       |

### Adaptors (lazy)

| Function                               | Description                                     | Docs                 |
| -------------------------------------- | ----------------------------------------------- | -------------------- |
| `iter_filter(it, pred, ctx)`           | Keep matching elements                          | [iter](docs/iter.md) |
| `iter_map(it, fn, ctx, out_size)`      | Transform each element                          | [iter](docs/iter.md) |
| `iter_take(it, n)`                     | Yield at most n elements                        | [iter](docs/iter.md) |
| `iter_take_while(it, pred, ctx)`       | Yield while pred holds, stop at first miss      | [iter](docs/iter.md) |
| `iter_skip(it, n)`                     | Skip first n elements                           | [iter](docs/iter.md) |
| `iter_skip_while(it, pred, ctx)`       | Skip while pred holds, yield the rest           | [iter](docs/iter.md) |
| `iter_chain(a, b)`                     | Concatenate two iterators                       | [iter](docs/iter.md) |
| `iter_zip(a, b)`                       | Interleave element pairs                        | [iter](docs/iter.md) |
| `iter_enumerate(it)`                   | Pair each element with its index                | [iter](docs/iter.md) |
| `iter_window(it, n)`                   | Sliding window of size n (yields Slice)         | [iter](docs/iter.md) |
| `iter_chunks(it, n)`                   | Non-overlapping chunks of size n (yields Slice) | [iter](docs/iter.md) |
| `iter_flat_map(it, fn, ctx, out_size)` | Expand each element into a sub-iterator         | [iter](docs/iter.md) |

### Terminals (consume)

| Function                        | Returns              | Docs                 |
| ------------------------------- | -------------------- | -------------------- |
| `iter_collect(it, al)`          | Arena-owned `Slice`  | [iter](docs/iter.md) |
| `iter_count(it)`                | `size_t` count       | [iter](docs/iter.md) |
| `iter_foreach(it, fn, ctx)`     | — (side-effects)     | [iter](docs/iter.md) |
| `iter_reduce(it, acc, fn, ctx)` | — (folds into acc)   | [iter](docs/iter.md) |
| `iter_sort(it, cmp, al)`        | Sorted `Slice`       | [iter](docs/iter.md) |
| `iter_find(it, pred, ctx, out)` | `bool` + first match | [iter](docs/iter.md) |
| `iter_any(it, pred, ctx)`       | `true` if any match  | [iter](docs/iter.md) |
| `iter_all(it, pred, ctx)`       | `true` if all match  | [iter](docs/iter.md) |
| `iter_min(it, cmp, out)`        | `bool` + minimum     | [iter](docs/iter.md) |
| `iter_max(it, cmp, out)`        | `bool` + maximum     | [iter](docs/iter.md) |

## Test coverage

Measured with `llvm-cov` (clang 18, instrumented build).
The test suite uses [Criterion](https://github.com/Snaipe/Criterion).

| Module    |   Lines | Functions | Branches |
| --------- | ------: | --------: | -------: |
| `arena`   |     91% |      100% |      78% |
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
| `btree`   |     92% |      100% |      73% |
| `avl`     |     93% |      100% |      76% |
| `omap`    |     88% |       97% |      67% |
| `pqueue`  |     98% |      100% |      80% |
| **Total** | **92%** |  **100%** |  **73%** |

The remaining branch gaps are defensive NULL-pointer guards and `align`
parameters that are always `_Alignof(max_align_t)` in practice.

## Known gaps / roadmap

### API / encapsulation (should fix before publishing)

- **PSL capped at `uint8_t` with no assertion** — a `psl` value > 255 wraps
  silently and corrupts the table. Add an `assert(psl < 255)` in the insert
  path so a degenerate hash function fails loudly instead of silently.

### Ergonomics (lower priority)

- **`iter_range` / `iter_generate` / `iter_from_slice` take an `Allocator` they
  never use** — these sources are stateless; the allocator is stored on `Iter`
  for adaptors that need it, but passing one here is awkward at the call site.

- **`list_pop_back` is O(n)** — documented, but the cost is easy to miss in a
  hot loop. Could add a note in the module docs.

### Future sources
- `iter_from_file` / `iter_lines` — I/O sources
- Cross-platform: `arena` uses `mmap`/`munmap` (`<sys/mman.h>`); needs `VirtualAlloc` path for Windows
