# stack

LIFO stack backed by a [`Vec`](vec.md).

**Header:** `src/stack/stack.h`  
**See also:** [`vec`](vec.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `Stack`

```c
typedef struct {
  Vec vec;
} Stack;
```

---

## Functions

### `stack_create`

```c
Stack stack_create(size_t elem_size, Allocator allocator);
```

Create an empty stack.

```c
Arena *a = arena_create(4096);
Stack  s = stack_create(sizeof(int), arena_allocator(a));
```

---

### `stack_push`

```c
void stack_push(Stack *s, const void *elem);
```

Push a copy of `elem` onto the top.

---

### `stack_pop`

```c
int stack_pop(Stack *s, void *out);
```

Remove the top element. Copies it into `*out` if `out` is not `NULL`.
Returns `1` on success, `0` if the stack is empty.

```c
int val;
if (stack_pop(&s, &val))
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
int    stack_is_empty(const Stack *s);
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

### `stack_free`

```c
void stack_free(Stack *s);
```

---

## Example

```c
Arena *a = arena_create(4096);
Stack  s = stack_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    stack_push(&s, &i);

// drain LIFO
int v;
while (stack_pop(&s, &v))
    printf("%d\n", v);  // prints 5 4 3 2 1

arena_free(a);
```
