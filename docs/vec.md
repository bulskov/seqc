# vec

Growable array backed by an arena-owned buffer.

**Header:** `src/vec/vec.h`  
**See also:** [`slice`](slice.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `Vec`

```c
typedef struct {
  void      *data;
  size_t     len;
  size_t     cap;
  size_t     elem_size;
  Allocator  allocator;
} Vec;
```

---

## Functions

### `vec_create`

```c
Vec vec_create(size_t elem_size, Allocator allocator);
```

Create an empty Vec with an initial small capacity.

```c
Arena *a = arena_create(4096);
Vec    v = vec_create(sizeof(int), arena_allocator(a));
```

### `vec_create_size`

```c
Vec vec_create_size(size_t elem_size, size_t capacity, Allocator allocator);
```

Create a Vec pre-allocated for at least `capacity` elements. Use this when the
final size is known in advance to avoid repeated reallocations.

---

### `vec_push`

```c
void vec_push(Vec *v, const void *elem);
```

Append a copy of `elem`. Reallocates if `len == cap`.

```c
for (int i = 0; i < 100; i++)
    vec_push(&v, &i);
```

---

### `vec_get`

```c
void *vec_get(const Vec *v, size_t i);
```

Return a pointer to element `i`. No bounds checking.

```c
int *third = vec_get(&v, 2);
```

---

### `vec_as_slice`

```c
Slice vec_as_slice(const Vec *v);
```

Return a non-owning [`Slice`](slice.md) view of the entire Vec buffer.

```c
Slice s = vec_as_slice(&v);
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

### `vec_free`

```c
void vec_free(Vec *v);
```

Release the Vec's buffer back to its allocator.

---

## Example

```c
Arena *a = arena_create(4096);
Vec    v = vec_create(sizeof(int), arena_allocator(a));

for (int i = 0; i < 10; i++)
    vec_push(&v, &i);

// forward
Iter fwd = vec_iter(&v);
int x;
while (fwd.next(&fwd, &x)) printf("%d ", x);
iter_drop(&fwd);

// reverse
Iter rev = vec_iter_rev(&v);
while (rev.next(&rev, &x)) printf("%d ", x);
iter_drop(&rev);

arena_free(a);
```
