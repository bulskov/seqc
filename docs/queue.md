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
growing_arena_t arena;
growing_arena_init(&arena, 4096);
allocator_t a = growing_arena_allocator(&arena);
queue_t *q = queue_create(sizeof(int), a);
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

### `queue_front_ptr`

```c
void *queue_front_ptr(const queue_t *q);
```

Return a pointer to the front element without removing it. Returns `NULL` if
empty.

---

### `queue_back_ptr`

```c
void *queue_back_ptr(const queue_t *q);
```

Return a pointer to the back (most recently enqueued) element without
removing it. Returns `NULL` if empty. Correctly handles the ring-buffer
wrap-around.

```c
queue_push(q, &(int){1});
queue_push(q, &(int){2});
queue_push(q, &(int){3});
printf("front=%d back=%d\n",
       *(int *)queue_front_ptr(q),   // 1
       *(int *)queue_back_ptr(q));  // 3
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

### `queue_destroy`

```c
void queue_destroy(queue_t *q);
```

Free the queue and all its internal storage. Do not use `q` after calling this.

---

### `queue_front` / `queue_back` and their `_ptr` variants

```c
seqc_status_t queue_front(const queue_t *q, void *out);
seqc_status_t queue_back(const queue_t *q, void *out);
void *queue_front_ptr(const queue_t *q);
void *queue_back_ptr(const queue_t *q);
```

The front element (what `queue_pop` returns next) and the back element (the last pushed). See [naming](naming.md#reading-elements-copy-or-pointer): the copy is yours, the pointer is valid only until the next change.

---

### `queue_extend`

```c
seqc_status_t queue_extend(queue_t *q, iter_t it);
```

Push every element of `it` at the back, in order. Add every element of `it`, which is always consumed — also on error and for a `NULL` collection (`SEQC_INVALID`). Stops at the first error and returns it.

---

## Example

```c
growing_arena_t arena;
growing_arena_init(&arena, 4096);
allocator_t a = growing_arena_allocator(&arena);
queue_t *q = queue_create(sizeof(int), a);

for (int i = 1; i <= 5; i++)
    queue_push(q, &i);

int v;
while (queue_pop(q, &v) == SEQC_OK)
    printf("%d\n", v);  // prints 1 2 3 4 5

growing_arena_destroy(&arena);
```
