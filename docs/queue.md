# queue

FIFO ring buffer.

**Header:** `src/queue/queue.h`  
**See also:** [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `Queue`

```c
typedef struct {
  char      *buf;
  size_t     cap;
  size_t     len;
  size_t     head;       /* index of front element */
  size_t     elem_size;
  Allocator  allocator;
} Queue;
```

Elements are stored in a flat circular buffer. The buffer doubles when full.

---

## Functions

### `queue_create`

```c
Queue queue_create(size_t elem_size, Allocator allocator);
```

Create an empty queue.

```c
Arena *a = arena_create(4096);
Queue  q = queue_create(sizeof(int), arena_allocator(a));
```

---

### `queue_push`

```c
void queue_push(Queue *q, const void *elem);
```

Enqueue a copy of `elem` at the back. Reallocates if full.

---

### `queue_pop`

```c
int queue_pop(Queue *q, void *out);
```

Dequeue the front element. Copies it into `*out` if `out` is not `NULL`.
Returns `1` on success, `0` if empty.

```c
int v;
while (queue_pop(&q, &v))
    printf("%d\n", v);
```

---

### `queue_peek`

```c
void *queue_peek(const Queue *q);
```

Return a pointer to the front element without removing it. Returns `NULL` if
empty.

---

### `queue_is_empty` / `queue_len`

```c
int    queue_is_empty(const Queue *q);
size_t queue_len(const Queue *q);
```

---

### `queue_iter`

```c
Iter queue_iter(const Queue *q);
```

Iterate from front to back without modifying the queue.

---

### `queue_free`

```c
void queue_free(Queue *q);
```

---

## Example

```c
Arena *a = arena_create(4096);
Queue  q = queue_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    queue_push(&q, &i);

int v;
while (queue_pop(&q, &v))
    printf("%d\n", v);  // prints 1 2 3 4 5

arena_free(a);
```
