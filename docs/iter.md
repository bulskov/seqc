# iter

Lazy iterator pipeline. Every data source exposes an `Iter`; adaptors transform
one `Iter` into another; terminals consume the iterator and produce a result.
Nothing allocates or runs until a terminal is called.

**Header:** `src/iter/iter.h`  
**See also:** [`arena`](arena.md) · [`slice`](slice.md) · [`vec`](vec.md)

---

## Type

### `Iter`

```c
struct Iter {
  bool (*next)(Iter *it, void *out); /* write elem_size bytes; return true/false */
  void (*drop)(Iter *it);            /* free internal state; NULL is valid       */
  void      *state;
  size_t     elem_size;
  Allocator  allocator;
};
```

Adaptors that need to allocate state (window, chunks, flat_map, …) use
`it.allocator`. The arena that owns the state is freed when `iter_drop()` is
called, or implicitly by the terminal.

### `iter_drop` (inline)

```c
static inline void iter_drop(Iter *it);
```

Release iterator resources without consuming elements. Call this when you want
to abandon an iterator early (e.g. after `iter_find`).

---

## Function-pointer types

| Type          | Signature                                                | Used by                                                                                  |
| ------------- | -------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| `pred_fn`     | `bool fn(const void *elem, void *ctx)`                   | `iter_filter`, `iter_find`, `iter_any`, `iter_all`, `iter_take_while`, `iter_skip_while` |
| `map_fn`      | `void fn(const void *in, void *out, void *ctx)`          | `iter_map`                                                                               |
| `combine_fn`  | `void fn(void *acc, const void *elem, void *ctx)`        | `iter_reduce`                                                                            |
| `visitor_fn`  | `void fn(const void *elem, void *ctx)`                   | `iter_foreach`                                                                           |
| `compare_fn`  | `int fn(const void *a, const void *b)`                   | `iter_sort`, `iter_min`, `iter_max`, sorted containers                                   |
| `flat_map_fn` | `void fn(const void *elem, Iter *out, void *ctx)`        | `iter_flat_map`                                                                          |  | `hash_fn` | `size_t fn(const void *key, size_t key_size)` | `hashmap_create`, `set_create` |
| `eq_fn`       | `bool fn(const void *a, const void *b, size_t key_size)` | `hashmap_create`, `set_create`                                                           |
---

## Sources

Sources create an `Iter` from an existing data structure.

### `iter_from_slice`

```c
Iter iter_from_slice(Slice s, Allocator allocator);
```

Iterate over a [`Slice`](slice.md) in order. `allocator` is inherited by
downstream adaptors.

```c
int nums[] = {1, 2, 3};
Slice s = {nums, 3, sizeof(int)};
Iter it = iter_from_slice(s, arena_allocator(a));
```

### `iter_from_slice_rev`

```c
Iter iter_from_slice_rev(Slice s, Allocator allocator);
```

Same as `iter_from_slice` but yields elements in reverse order.

---

### `iter_generate`

```c
typedef bool (*generate_fn)(void *out, void *ctx);
Iter iter_generate(generate_fn fn, void *ctx, size_t elem_size,
                   Allocator allocator);
```

Create an iterator that calls `fn(out, ctx)` on each `next` call. `fn` writes
`elem_size` bytes into `out` and returns `true` to continue or `false` to
signal exhaustion. Useful for stateful generators or wrapping external sources.

```c
/* Fibonacci generator */
typedef struct { long long a, b; } FibState;

static bool fib_next(void *out, void *ctx) {
    FibState *s = ctx;
    *(long long *)out = s->a;
    long long tmp = s->a + s->b;
    s->a = s->b;
    s->b = tmp;
    return true;  /* infinite — pair with iter_take */
}

FibState state = {0, 1};
Slice first10 = iter_collect(
    iter_take(iter_generate(fib_next, &state, sizeof(long long),
                            arena_allocator(a)), 10),
    arena_allocator(a));
```

---

### `iter_range`

```c
Iter iter_range(long long start, long long end, long long step,
                Allocator allocator);
```

Yield `long long` integers from `start` (inclusive) to `end` (exclusive) with
the given `step`. If `step` is zero an empty iterator is returned. Yields
nothing if the range is already exhausted (e.g. `start >= end` with positive
step).

```c
/* 0, 1, 2, 3, 4 */
Iter it = iter_range(0, 5, 1, arena_allocator(a));

/* 10, 8, 6, 4, 2 */
Iter it = iter_range(10, 0, -2, arena_allocator(a));
```

Other sources live on their respective modules:

| Source                                               | Module                |
| ---------------------------------------------------- | --------------------- |
| `vec_iter` / `vec_iter_rev`                          | [vec](vec.md)         |
| `stack_iter`                                         | [stack](stack.md)     |
| `queue_iter`                                         | [queue](queue.md)     |
| `list_iter`                                          | [list](list.md)       |
| `dlist_iter` / `dlist_iter_reverse`                  | [dlist](dlist.md)     |
| `set_iter`                                           | [set](set.md)         |
| `iter_from_hashmap`                                  | [hashmap](hashmap.md) |
| `string_chars` / `string_chars_rev` / `string_split` | [string](string.md)   |
| `btree_iter` / `btree_iter_rev` / `btree_iter_range` | [btree](btree.md)     |
| `avl_iter` / `avl_iter_rev` / `avl_iter_range`       | [avl](avl.md)         |
| `omap_iter` / `omap_iter_rev` / `omap_iter_range`    | [omap](omap.md)       |

---

## Adaptors

Adaptors take ownership of their source `Iter` and return a new one. They are
lazy — no work happens until `next()` is called.

### `iter_filter`

```c
Iter iter_filter(Iter source, pred_fn pred, void *ctx);
```

Yield only elements for which `pred(elem, ctx)` returns non-zero.

```c
static int is_positive(const void *e, void *ctx) {
    return *(const int *)e > 0;
}

Iter it = iter_filter(vec_iter(&v), is_positive, NULL);
```

---

### `iter_map`

```c
Iter iter_map(Iter source, map_fn map, void *ctx, size_t out_elem_size);
```

Transform each element. The output type may differ from the input type;
`out_elem_size` must match the size written by `map`.

```c
static void to_double(const void *in, void *out, void *ctx) {
    *(double *)out = (double)*(const int *)in;
}

Iter it = iter_map(vec_iter(&v), to_double, NULL, sizeof(double));
```

---

### `iter_take`

```c
Iter iter_take(Iter source, size_t n);
```

Yield at most `n` elements then stop.

---

### `iter_skip`

```c
Iter iter_skip(Iter source, size_t n);
```

Discard the first `n` elements then yield the rest.

---

### `iter_take_while`

```c
Iter iter_take_while(Iter source, pred_fn pred, void *ctx);
```

Yield elements from `source` as long as `pred` returns `true`. Stops
permanently at the first element that fails — that element is not yielded.

```c
static bool is_positive(const void *elem, void *ctx) {
    return *(const int *)elem > 0;
}

// {3, 7, -1, 5} → yields 3, 7  (stops at -1)
Iter it = iter_take_while(vec_iter(&v), is_positive, NULL);
```

---

### `iter_skip_while`

```c
Iter iter_skip_while(Iter source, pred_fn pred, void *ctx);
```

Skip elements from `source` as long as `pred` returns `true`. Once the first
element fails the predicate, that element and all subsequent elements are
yielded unconditionally (even if they would satisfy the predicate).

```c
static bool is_positive(const void *elem, void *ctx) {
    return *(const int *)elem > 0;
}

// {3, 7, -1, 5} → yields -1, 5  (skips 3 and 7)
Iter it = iter_skip_while(vec_iter(&v), is_positive, NULL);
```

---

### `iter_chain`

```c
Iter iter_chain(Iter a, Iter b);
```

Yield all elements of `a`, then all elements of `b`. Both iterators must have
the same `elem_size`.

```c
Iter combined = iter_chain(list_iter(&left), list_iter(&right));
```

---

### `iter_zip`

```c
Iter iter_zip(Iter a, Iter b);
```

Yield interleaved pairs `[a_elem | b_elem]` as a flat buffer. Stops when either
source is exhausted. The result `elem_size` equals `a.elem_size + b.elem_size`.

```c
// zip int keys with double values
Iter pairs = iter_zip(keys_iter, vals_iter);
// each element is {int, double} — cast accordingly
```

---

### `iter_enumerate`

```c
Iter iter_enumerate(Iter source);
```

Pair each element with its 0-based index. Yields `EnumEntry`:

```c
typedef struct {
    size_t index;
    void  *elem;  /* pointer into internal buffer — valid until next call */
} EnumEntry;
```

```c
Iter it = iter_enumerate(list_iter(&l));
EnumEntry e;
while (it.next(&it, &e))
    printf("[%zu] %d\n", e.index, *(int *)e.elem);
iter_drop(&it);
```

---

## Shared entry types

### `MapEntry`

```c
typedef struct {
    void *key;
    void *value;
} MapEntry;
```

Pointer-pair yielded by [`hashmap_iter`](hashmap.md#hashmap_iter) and
[`omap_iter`](omap.md#omap_iter). Both pointers point directly into live
bucket / node storage. Do not modify the map while iterating.

`hashmap.h` and `omap.h` each export a typedef alias
(`HashMapEntry` / `OMapEntry`) for callers that prefer the module-prefixed
name.

---

### `iter_window`

```c
Iter iter_window(Iter source, size_t n);
```

Yield overlapping windows of size `n` as a [`Slice`](slice.md) pointing into an
internal buffer. **Copy the slice if you need to keep it past the next call.**
Yields nothing if the source has fewer than `n` elements.

```c
// sliding 3-element window over [1,2,3,4,5]
Iter it = iter_window(vec_iter(&v), 3);
Slice win;
while (it.next(&it, &win))
    printf("%d %d %d\n", *(int *)slice_get(win,0),
                         *(int *)slice_get(win,1),
                         *(int *)slice_get(win,2));
iter_drop(&it);
```

---

### `iter_chunks`

```c
Iter iter_chunks(Iter source, size_t n);
```

Yield non-overlapping chunks of exactly `n` elements as a [`Slice`](slice.md).
The last chunk may be smaller than `n`. **Copy the slice if you need to keep it
past the next call.**

---

### `iter_flat_map`

```c
typedef void (*flat_map_fn)(const void *elem, Iter *out, void *ctx);
Iter iter_flat_map(Iter source, flat_map_fn fn, void *ctx,
                   size_t out_elem_size);
```

For each element in `source`, call `fn(elem, &sub, ctx)` which must populate
`*sub` with a sub-iterator. All sub-iterators are drained in order.
`out_elem_size` must match the `elem_size` of every sub-iterator produced.

```c
static void expand(const void *elem, Iter *out, void *ctx) {
    // produce a Vec of ints from each input int, then iterate it
    int n = *(const int *)elem;
    Vec *v = ctx;
    vec_push(v, &n);
    vec_push(v, &n);
    *out = vec_iter(v);
}
```

---

## Terminals

Terminals consume the iterator and free its resources.

### `iter_collect` {#iter_collect}

```c
Slice iter_collect(Iter it, Allocator allocator);
```

Materialise all elements into an arena-owned [`Slice`](slice.md). `allocator`
determines which arena owns the returned memory. Pass the same arena you used
for the iterator chain, or any other arena whose lifetime exceeds the slice.

```c
Arena *a = arena_create(4096);
Slice s = iter_collect(
    iter_map(vec_iter(&v), double_fn, NULL, sizeof(double)),
    arena_allocator(a));
```

---

### `iter_count`

```c
size_t iter_count(Iter it);
```

Consume the iterator and return the number of elements yielded.

---

### `iter_foreach`

```c
void iter_foreach(Iter it, visitor_fn visit, void *ctx);
```

Call `visit(elem, ctx)` for every element. Useful when side-effects are all
you need.

```c
static void print_int(const void *e, void *ctx) {
    printf("%d\n", *(const int *)e);
}
iter_foreach(vec_iter(&v), print_int, NULL);
```

---

### `iter_reduce`

```c
void iter_reduce(Iter it, void *acc, combine_fn combine, void *ctx);
```

Fold all elements into `acc`. `combine(acc, elem, ctx)` is called for each
element; the caller initialises `acc` before calling.

```c
static void sum(void *acc, const void *elem, void *ctx) {
    *(int *)acc += *(const int *)elem;
}

int total = 0;
iter_reduce(vec_iter(&v), &total, sum, NULL);
```

---

### `iter_sort` {#iter_sort}

```c
Slice iter_sort(Iter it, compare_fn cmp, Allocator allocator);
```

Collect all elements into `allocator` then sort in place using `cmp`. Returns
the sorted [`Slice`](slice.md).

```c
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}
Arena *a = arena_create(4096);
Slice sorted = iter_sort(vec_iter(&v), int_cmp, arena_allocator(a));
```

---

### `iter_find`

```c
bool iter_find(Iter it, pred_fn pred, void *ctx, void *out);
```

Return `true` and write the first matching element to `*out` (may be `NULL`) if
found; return `false` otherwise. The iterator is dropped after use.

---

### `iter_any`

```c
bool iter_any(Iter it, pred_fn pred, void *ctx);
```

Return `true` if any element satisfies `pred`. Short-circuits on first match.

---

### `iter_all`

```c
bool iter_all(Iter it, pred_fn pred, void *ctx);
```

Return `true` if every element satisfies `pred` (vacuously true for empty
iterators). Short-circuits on first failure.

---

### `iter_min`

```c
bool iter_min(Iter it, compare_fn cmp, void *out);
```

Write the minimum element to `*out` (`out` may be `NULL`). Returns `true` on
success, `false` if the iterator was empty.

---

### `iter_max`

```c
bool iter_max(Iter it, compare_fn cmp, void *out);
```

Write the maximum element to `*out` (`out` may be `NULL`). Returns `true` on
success, `false` if the iterator was empty.

---

## Full pipeline example

```c
#include "arena/arena.h"
#include "vec/vec.h"
#include "iter/iter.h"

static bool is_even(const void *e, void *ctx)  { return *(const int *)e % 2 == 0; }
static void triple(const void *in, void *out, void *ctx) {
    *(int *)out = *(const int *)in * 3;
}
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

int main(void) {
    Arena *a = arena_create(4096);
    Vec    v = vec_create(sizeof(int), arena_allocator(a));

    for (int i = 10; i >= 1; i--) vec_push(&v, &i);

    // keep evens, triple, sort ascending
    Slice result = iter_sort(
                       iter_map(
                           iter_filter(vec_iter(&v), is_even, NULL),
                           triple, NULL, sizeof(int)),
                       int_cmp, arena_allocator(a));
    // result = {6, 12, 18, 24, 30}

    arena_free(a);
}
```
