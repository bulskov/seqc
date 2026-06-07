# queue

FIFO ring buffer.

**Header:** `include/seqc/queue.h`  
**See also:** [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `queue_t`

```c
typedef struct queue_t queue_t;
```

Opaque handle. Backed internally by a flat circular buffer that doubles when
full.

---

## Functions

### `queue_create`

```c
queue_t *queue_create(size_t elem_size, allocator_t allocator);
```

Create an empty queue. Returns `NULL` if `elem_size` is zero.

```c
Arena *a = arena_create(4096);
queue_t *q = queue_create(sizeof(int), arena_allocator(a));
```

---

### `queue_push`

```c
seqc_status_t queue_push(queue_t *q, const void *elem);
```

Enqueue a copy of `elem` at the back. Reallocates if full. Returns `SEQC_OOM`
on allocation failure; `SEQC_OK` otherwise.

---

### `queue_pop`

```c
seqc_status_t queue_pop(queue_t *q, void *out);
```

Dequeue the front element. Copies it into `*out` if `out` is not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if empty.

```c
int v;
while (queue_pop(q, &v) == SEQC_OK)
    printf("%d\n", v);
```

---

### `queue_peek`

```c
void *queue_peek(const queue_t *q);
```

Return a pointer to the front element without removing it. Returns `NULL` if
empty.

---

### `queue_back`

```c
void *queue_back(const queue_t *q);
```

Return a pointer to the back (most recently enqueued) element without
removing it. Returns `NULL` if empty. Correctly handles the ring-buffer
wrap-around.

```c
queue_push(q, &(int){1});
queue_push(q, &(int){2});
queue_push(q, &(int){3});
printf("front=%d back=%d\n",
       *(int *)queue_peek(q),   // 1
       *(int *)queue_back(q));  // 3
```

---

### `queue_is_empty` / `queue_len`

```c
bool   queue_is_empty(const queue_t *q);
size_t queue_len(const queue_t *q);
```

---

### `queue_iter`

```c
iter_t queue_iter(const queue_t *q);
```

Iterate from front to back without modifying the queue.

---

### `queue_iter_rev`

```c
iter_t queue_iter_rev(const queue_t *q);
```

Iterate from back to front. Correctly handles the ring-buffer wrap-around.

---

### `queue_clear`

```c
void queue_clear(queue_t *q);
```

Empty the queue. The ring-buffer is retained and `head` is reset to zero.

---

### `queue_free`

```c
void queue_free(queue_t *q);
```

Free the queue and all its internal storage. Do not use `q` after calling this.

---

## Example

```c
Arena *a = arena_create(4096);
queue_t *q = queue_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    queue_push(q, &i);

int v;
while (queue_pop(q, &v) == SEQC_OK)
    printf("%d\n", v);  // prints 1 2 3 4 5

arena_free(a);
```
