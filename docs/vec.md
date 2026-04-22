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

### `vec_pop`

```c
bool vec_pop(Vec *v, void *out);
```

Remove the last element. Copies it into `*out` if `out` is not `NULL`.
Returns `true` on success, `false` if the Vec is empty.

```c
int val;
while (vec_pop(&v, &val))
    printf("%d\n", val);
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

### `vec_set`

```c
void vec_set(Vec *v, size_t i, const void *elem);
```

Overwrite the element at index `i` with a copy of `*elem`. No-op if `i >= len`.

```c
int val = 99;
vec_set(&v, 0, &val);
```

---

### `vec_insert`

```c
void vec_insert(Vec *v, size_t i, const void *elem);
```

Insert a copy of `*elem` at index `i`, shifting all elements from `i` onward
one position to the right. `i == len` is equivalent to `vec_push`. Reallocates
if necessary.

```c
int zero = 0;
vec_insert(&v, 0, &zero);  /* prepend */
```

---

### `vec_remove`

```c
void vec_remove(Vec *v, size_t i);
```

Remove the element at index `i`, shifting elements from `i+1` onward one
position to the left. No-op if `i >= len`.

```c
vec_remove(&v, 0);  /* remove first element */
```

---

### `vec_reserve`

```c
void vec_reserve(Vec *v, size_t capacity);
```

Ensure the Vec has room for at least `capacity` elements without reallocating.
Does nothing if `cap` is already sufficient.

```c
vec_reserve(&v, 1024);  /* pre-allocate space */
for (int i = 0; i < 1000; i++) vec_push(&v, &i);  /* no reallocs */
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
