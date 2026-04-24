# stack

LIFO stack backed by a [`Vec`](vec.md).

**Header:** `src/stack/stack.h`  
**See also:** [`vec`](vec.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `Stack`

```c
typedef struct Stack Stack;
```

Opaque handle. Backed internally by a [`Vec`](vec.md).

---

## Functions

### `stack_create`

```c
Stack *stack_create(size_t elem_size, Allocator allocator);
```

Create an empty stack. Returns `NULL` if `elem_size` is zero.

```c
Arena *a = arena_create(4096);
Stack *s = stack_create(sizeof(int), arena_allocator(a));
```

---

### `stack_push`

```c
SeqcStatus stack_push(Stack *s, const void *elem);
```

Push a copy of `elem` onto the top. Returns `SEQC_OOM` if the underlying Vec
fails to grow; `SEQC_OK` otherwise.

---

### `stack_pop`

```c
SeqcStatus stack_pop(Stack *s, void *out);
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
void *stack_peek(const Stack *s);
```

Return a pointer to the top element without removing it. Returns `NULL` if
the stack is empty.

---

### `stack_is_empty` / `stack_len`

```c
bool   stack_is_empty(const Stack *s);
size_t stack_len(const Stack *s);
```

---

### `stack_iter`

```c
Iter stack_iter(const Stack *s);
```

Iterate from **bottom to top** (insertion order). For most stack use cases you
want to `pop` elements one at a time rather than iterate.

---

### `stack_iter_rev`

```c
Iter stack_iter_rev(const Stack *s);
```

Iterate from **top to bottom** (reverse insertion order — LIFO order).

---

### `stack_clear`

```c
void stack_clear(Stack *s);
```

Empty the stack. The underlying Vec buffer is retained.

---

### `stack_free`

```c
void stack_free(Stack *s);
```

Free the Stack and all its internal storage. Do not use `s` after calling this.

---

## Example

```c
Arena *a = arena_create(4096);
Stack *s = stack_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    stack_push(s, &i);

// drain LIFO
int v;
while (stack_pop(s, &v) == SEQC_OK)
    printf("%d\n", v);  // prints 5 4 3 2 1

arena_free(a);
```
