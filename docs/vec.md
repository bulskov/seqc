# vec

Growable array backed by an arena-owned buffer.

**Header:** `src/vec/vec.h`  
**See also:** [`slice`](slice.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `Vec`

```c
typedef struct Vec Vec;
```

Opaque handle. All fields are private — use the accessor functions below.

---

## Functions

### `vec_create`

```c
Vec *vec_create(size_t elem_size, Allocator allocator);
```

Create an empty Vec with an initial small capacity. Returns `NULL` if
`elem_size` is zero.

```c
Arena *a = arena_create(4096);
Vec   *v = vec_create(sizeof(int), arena_allocator(a));
```

### `vec_create_size`

```c
Vec *vec_create_size(size_t elem_size, size_t capacity, Allocator allocator);
```

Create a Vec pre-allocated for at least `capacity` elements. Use this when the
final size is known in advance to avoid repeated reallocations.

---

### `vec_len` / `vec_cap` / `vec_elem_size`

```c
size_t vec_len(const Vec *v);
size_t vec_cap(const Vec *v);
size_t vec_elem_size(const Vec *v);
```

Accessors for the three key properties of the Vec.

---

### `vec_push`

```c
SeqcStatus vec_push(Vec *v, const void *elem);
```

Append a copy of `elem`. Reallocates if `len == cap`. Returns `SEQC_OOM` if
the reallocation fails (the Vec is unchanged); `SEQC_OK` otherwise.

```c
for (int i = 0; i < 100; i++) {
    if (vec_push(v, &i) != SEQC_OK) { /* handle OOM */ }
}
```

---

### `vec_pop`

```c
bool vec_pop(Vec *v, void *out);
```

Remove the last element. Copies it into `*out` if `out` is not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if the Vec is empty.

```c
int val;
while (vec_pop(v, &val) == SEQC_OK)
    printf("%d\n", val);
```

---

### `vec_get`

```c
void *vec_get(const Vec *v, size_t i);
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
SeqcStatus vec_get_copy(const Vec *v, size_t i, void *out);
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
void vec_set(Vec *v, size_t i, const void *elem);
```

Overwrite the element at index `i` with a copy of `*elem`. No-op if `i >= len`.

```c
int val = 99;
vec_set(v, 0, &val);
```

---

### `vec_insert`

```c
SeqcStatus vec_insert(Vec *v, size_t i, const void *elem);
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
void vec_remove(Vec *v, size_t i);
```

Remove the element at index `i`, shifting elements from `i+1` onward one
position to the left. No-op if `i >= len`.

```c
vec_remove(v, 0);  /* remove first element */
```

---

### `vec_reserve`

```c
SeqcStatus vec_reserve(Vec *v, size_t capacity);
```

Ensure the Vec has room for at least `capacity` elements without reallocating.
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
Slice vec_as_slice(const Vec *v);
```

Return a non-owning [`Slice`](slice.md) view of the entire Vec buffer.

```c
Slice s = vec_as_slice(v);
Slice sorted = iter_sort(iter_from_slice(s, arena_allocator(a)), int_cmp);
```

---

### `vec_iter`

```c
Iter vec_iter(const Vec *v);
```

Create a forward [`Iter`](iter.md) over the Vec.

### `vec_iter_rev`

```c
Iter vec_iter_rev(const Vec *v);
```

Create a reverse [`Iter`](iter.md) over the Vec (last element first).

---

### `vec_find`

```c
void *vec_find(const Vec *v, pred_fn pred, void *ctx);
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
bool vec_contains(const Vec *v, pred_fn pred, void *ctx);
```

Return `true` if any element satisfies `pred`. Equivalent to
`vec_find(v, pred, ctx) != NULL`.

---

### `vec_clear`

```c
void vec_clear(Vec *v);
```

Reset `len` to zero. The allocated buffer is retained, so subsequent pushes
will not reallocate until capacity is exhausted again.

---

### `vec_free`

```c
void vec_free(Vec *v);
```

Free the Vec's buffer and then the Vec struct itself. Do not use `v` after
calling this.

---

### `vec_extend`

```c
SeqcStatus vec_extend(Vec *v, Iter it);
```

Drain `it`, pushing each element into `v`. Stops and returns `SEQC_OOM` on
the first allocation failure; all elements pushed before the failure remain.
The iterator is always dropped before returning.

```c
Vec *src = /* ... */;
Vec *dst = vec_create(sizeof(int), arena_allocator(a));
vec_extend(dst, vec_iter(src));  /* copy all elements from src */
```

---

## Example

```c
Arena *a = arena_create(4096);
Vec   *v = vec_create(sizeof(int), arena_allocator(a));

for (int i = 0; i < 10; i++)
    vec_push(v, &i);

// forward
Iter fwd = vec_iter(v);
int x;
while (fwd.next(&fwd, &x)) printf("%d ", x);
iter_drop(&fwd);

// reverse
Iter rev = vec_iter_rev(v);
while (rev.next(&rev, &x)) printf("%d ", x);
iter_drop(&rev);

arena_free(a);
```
