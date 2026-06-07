# vec

Growable array backed by an arena-owned buffer.

**Header:** `include/seqc/vec.h`  
**See also:** [`slice`](slice.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `vec_t`

```c
typedef struct vec_t vec_t;
```

Opaque handle. All fields are private — use the accessor functions below.

---

## Functions

### `vec_create`

```c
vec_t *vec_create(size_t elem_size, allocator_t allocator);
```

Create an empty vec_t with an initial small capacity. Returns `NULL` if
`elem_size` is zero.

```c
Arena *a = arena_create(4096);
vec_t   *v = vec_create(sizeof(int), arena_allocator(a));
```

### `vec_create_size`

```c
vec_t *vec_create_size(size_t elem_size, size_t capacity, allocator_t allocator);
```

Create a vec_t pre-allocated for at least `capacity` elements. Use this when the
final size is known in advance to avoid repeated reallocations.

---

### `vec_len` / `vec_cap` / `vec_elem_size`

```c
size_t vec_len(const vec_t *v);
size_t vec_cap(const vec_t *v);
size_t vec_elem_size(const vec_t *v);
```

Accessors for the three key properties of the vec_t.

---

### `vec_push`

```c
seqc_status_t vec_push(vec_t *v, const void *elem);
```

Append a copy of `elem`. Reallocates if `len == cap`. Returns `SEQC_OOM` if
the reallocation fails (the vec_t is unchanged); `SEQC_OK` otherwise.

```c
for (int i = 0; i < 100; i++) {
    if (vec_push(v, &i) != SEQC_OK) { /* handle OOM */ }
}
```

---

### `vec_pop`

```c
seqc_status_t vec_pop(vec_t *v, void *out);
```

Remove the last element. Copies it into `*out` if `out` is not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if the vec_t is empty.

```c
int val;
while (vec_pop(v, &val) == SEQC_OK)
    printf("%d\n", val);
```

---

### `vec_get`

```c
void *vec_get(const vec_t *v, size_t i);
```

Return a pointer to element `i`. No bounds checking. The pointer is
invalidated by any operation that reallocates the buffer (`vec_push`,
`vec_insert`, `vec_reserve`).

```c
int *third = vec_get(v, 2);
```

---

### `vec_get_copy`

```c
seqc_status_t vec_get_copy(const vec_t *v, size_t i, void *out);
```

Copy element `i` into `*out`. Returns `SEQC_OK` on success,
`SEQC_NOT_FOUND` if `i >= len`, or `SEQC_INVALID` if `out` is `NULL`.
Unlike `vec_get`, the copied value is not invalidated by later reallocations.

```c
int val;
if (vec_get_copy(v, 2, &val) == SEQC_OK)
    printf("%d\n", val);
```

---

### `vec_set`

```c
void vec_set(vec_t *v, size_t i, const void *elem);
```

Overwrite the element at index `i` with a copy of `*elem`. No-op if `i >= len`.

```c
int val = 99;
vec_set(v, 0, &val);
```

---

### `vec_insert`

```c
seqc_status_t vec_insert(vec_t *v, size_t i, const void *elem);
```

Insert a copy of `*elem` at index `i`, shifting all elements from `i` onward
one position to the right. `i == len` is equivalent to `vec_push`. Reallocates
if necessary. Returns `SEQC_OOM` if reallocation fails; `SEQC_OK` otherwise.

```c
int zero = 0;
vec_insert(v, 0, &zero);  /* prepend */
```

---

### `vec_remove`

```c
void vec_remove(vec_t *v, size_t i);
```

Remove the element at index `i`, shifting elements from `i+1` onward one
position to the left. No-op if `i >= len`.

```c
vec_remove(v, 0);  /* remove first element */
```

---

### `vec_reserve`

```c
seqc_status_t vec_reserve(vec_t *v, size_t capacity);
```

Ensure the vec_t has room for at least `capacity` elements without reallocating.
Does nothing and returns `SEQC_OK` if `cap` is already sufficient.
Returns `SEQC_OOM` if the reallocation fails.

```c
if (vec_reserve(v, 1024) == SEQC_OK) {
    for (int i = 0; i < 1000; i++) vec_push(v, &i);  /* no reallocs */
}
```

---

### `vec_as_slice`

```c
slice_t vec_as_slice(const vec_t *v);
```

Return a non-owning [`slice_t`](slice.md) view of the entire vec_t buffer.

```c
slice_t s = vec_as_slice(v);
slice_t sorted = iter_sort(iter_from_slice(s, arena_allocator(a)), int_cmp, arena_allocator(a));
```

---

### `vec_iter`

```c
iter_t vec_iter(const vec_t *v);
```

Create a forward [`iter_t`](iter.md) over the vec_t.

### `vec_iter_rev`

```c
iter_t vec_iter_rev(const vec_t *v);
```

Create a reverse [`iter_t`](iter.md) over the vec_t (last element first).

---

### `vec_find`

```c
void *vec_find(const vec_t *v, pred_fn pred, void *ctx);
```

Return a pointer to the first element for which `pred` returns `true`, or
`NULL` if no such element exists. Linear scan.

```c
static bool is_negative(const void *elem, void *ctx) {
    (void)ctx;
    return *(const int *)elem < 0;
}

int *p = vec_find(&v, is_negative, NULL);
if (p) printf("first negative: %d\n", *p);
```

### `vec_contains`

```c
bool vec_contains(const vec_t *v, pred_fn pred, void *ctx);
```

Return `true` if any element satisfies `pred`. Equivalent to
`vec_find(v, pred, ctx) != NULL`.

---

### `vec_clear`

```c
void vec_clear(vec_t *v);
```

Reset `len` to zero. The allocated buffer is retained, so subsequent pushes
will not reallocate until capacity is exhausted again.

---

### `vec_free`

```c
void vec_free(vec_t *v);
```

Free the vec_t's buffer and then the vec_t struct itself. Do not use `v` after
calling this.

---

### `vec_extend`

```c
seqc_status_t vec_extend(vec_t *v, iter_t it);
```

Drain `it`, pushing each element into `v`. Stops and returns `SEQC_OOM` on
the first allocation failure; all elements pushed before the failure remain.
The iterator is always dropped before returning.

```c
vec_t *src = /* ... */;
vec_t *dst = vec_create(sizeof(int), arena_allocator(a));
vec_extend(dst, vec_iter(src));  /* copy all elements from src */
```

---

### `vec_sort`

```c
void vec_sort(vec_t *v, compare_fn cmp);
```

Sort the elements of `v` in-place using `qsort`. No allocation. `cmp` follows
the same sign convention as `iter_sort`. A NULL `v`, NULL `cmp`, or a vec with
fewer than two elements is a no-op.

```c
vec_sort(v, int_cmp);  /* sort in-place; vec's buffer is modified directly */
```

---

## Example

```c
Arena *a = arena_create(4096);
vec_t   *v = vec_create(sizeof(int), arena_allocator(a));

for (int i = 0; i < 10; i++)
    vec_push(v, &i);

// forward
iter_t fwd = vec_iter(v);
int x;
while (fwd.next(&fwd, &x)) printf("%d ", x);
iter_drop(&fwd);

// reverse
iter_t rev = vec_iter_rev(v);
while (rev.next(&rev, &x)) printf("%d ", x);
iter_drop(&rev);

arena_free(a);
```
