# slice

A non-owning view into a contiguous block of typed elements.

**Header:** `include/seqc/slice.h`  
**See also:** [`iter`](iter.md) · [`vec`](vec.md) · [`arena`](arena.md)

---

## Type

### `slice_t`

```c
typedef struct {
  void  *ptr;
  size_t len;
  size_t elem_size;
} slice_t;
```

Fat pointer: base address + element count + element size. Does not own the
memory it points to. Produced by [`iter_collect()`](iter.md#iter_collect),
[`iter_sort()`](iter.md#iter_sort), [`vec_as_slice()`](vec.md#vec_as_slice),
and other terminals.

---

## Functions

### `slice_get`

```c
void *slice_get(slice_t s, size_t i);
```

Return a pointer to element `i`. No bounds checking.

```c
slice_t s = iter_collect(iter_from_slice(original, al));

for (size_t i = 0; i < s.len; i++) {
    int *val = slice_get(s, i);
    printf("%d\n", *val);
}
```

---

### `slice_find`

```c
void *slice_find(slice_t s, bool (*pred)(const void *elem, void *ctx), void *ctx);
```

Return a pointer to the first element for which `pred` returns `true`, or
`NULL` if no such element exists. Linear scan.

```c
static bool is_negative(const void *elem, void *ctx) {
    (void)ctx;
    return *(const int *)elem < 0;
}

int nums[] = {3, -1, 4, -2};
slice_t s = {nums, 4, sizeof(int)};
int *p = slice_find(s, is_negative, NULL);  // points to -1
```

### `slice_contains`

```c
bool slice_contains(slice_t s, bool (*pred)(const void *elem, void *ctx), void *ctx);
```

Return `true` if any element satisfies `pred`. Equivalent to
`slice_find(s, pred, ctx) != NULL`.

---

## Creating a slice

Slices are returned by:

| Source | How |
|--------|-----|
| `iter_collect(it)` | Materialise any iterator into an arena-owned slice |
| `iter_sort(it, cmp)` | Collect + sort in one step |
| `vec_as_slice(&v)` | Zero-copy view of a vec_t's buffer |
| Literal `(slice_t){ptr, len, elem_size}` | Wrap any contiguous buffer |

```c
// wrap a C array
int nums[] = {3, 1, 4, 1, 5};
slice_t s = {nums, 5, sizeof(int)};

// iterate it
iter_t it = iter_from_slice(s, arena_allocator(a));
```
