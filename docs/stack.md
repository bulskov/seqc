# stack

LIFO stack backed by a [`vec_t`](vec.md).

**Header:** `include/seqc/stack.h`  
**See also:** [`vec`](vec.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `seqc_stack_t`

```c
typedef struct seqc_stack_t seqc_stack_t;
```

Opaque handle. Backed internally by a [`vec_t`](vec.md).

---

## Functions

### `stack_create`

```c
seqc_stack_t *stack_create(size_t elem_size, allocator_t allocator);
```

Create an empty stack. Returns `NULL` if `elem_size` is zero.

```c
Arena *a = arena_create(4096);
seqc_stack_t *s = stack_create(sizeof(int), arena_allocator(a));
```

---

### `stack_push`

```c
seqc_status_t stack_push(seqc_stack_t *s, const void *elem);
```

Push a copy of `elem` onto the top. Returns `SEQC_OOM` if the underlying vec_t
fails to grow; `SEQC_OK` otherwise.

---

### `stack_pop`

```c
seqc_status_t stack_pop(seqc_stack_t *s, void *out);
```

Remove the top element. Copies it into `*out` if `out` is not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if the stack is empty.

```c
int val;
if (stack_pop(s, &val) == SEQC_OK)
    printf("popped %d\n", val);
```

---

### `stack_peek`

```c
void *stack_peek(const seqc_stack_t *s);
```

Return a pointer to the top element without removing it. Returns `NULL` if
the stack is empty.

---

### `stack_is_empty` / `stack_len`

```c
bool   stack_is_empty(const seqc_stack_t *s);
size_t stack_len(const seqc_stack_t *s);
```

---

### `stack_iter`

```c
iter_t stack_iter(const seqc_stack_t *s);
```

Iterate from **bottom to top** (insertion order). For most stack use cases you
want to `pop` elements one at a time rather than iterate.

---

### `stack_iter_rev`

```c
iter_t stack_iter_rev(const seqc_stack_t *s);
```

Iterate from **top to bottom** (reverse insertion order — LIFO order).

---

### `stack_clear`

```c
void stack_clear(seqc_stack_t *s);
```

Empty the stack. The underlying vec_t buffer is retained.

---

### `stack_free`

```c
void stack_free(seqc_stack_t *s);
```

Free the stack and all its internal storage. Do not use `s` after calling this.

---

## Example

```c
Arena *a = arena_create(4096);
seqc_stack_t *s = stack_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    stack_push(s, &i);

// drain LIFO
int v;
while (stack_pop(s, &v) == SEQC_OK)
    printf("%d\n", v);  // prints 5 4 3 2 1

arena_free(a);
```
