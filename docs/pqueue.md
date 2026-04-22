# pqueue

Binary min-heap priority queue. The element with the lowest
[`compare_fn`](iter.md#function-pointer-types) value is always at the front.
All push/pop operations are O(log n); peek is O(1).

**Header:** `src/pqueue/pqueue.h`  
**See also:** [`vec`](vec.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `PQueue`

```c
typedef struct {
  Vec        data;   /* flat array storage */
  compare_fn cmp;
} PQueue;
```

Backed by a [`Vec`](vec.md). Elements are stored contiguously; the heap
property is maintained by sift-up on push and sift-down on pop.

---

## Functions

### `pqueue_create`

```c
PQueue pqueue_create(size_t elem_size, compare_fn cmp, Allocator allocator);
```

Create an empty priority queue.

```c
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

Arena  *a = arena_create(4096);
PQueue  q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
```

To get a **max-heap**, pass a negated comparator:

```c
static int int_cmp_desc(const void *a, const void *b) {
    return *(const int *)b - *(const int *)a;
}
PQueue max_heap = pqueue_create(sizeof(int), int_cmp_desc, arena_allocator(a));
```

---

### `pqueue_push`

```c
void pqueue_push(PQueue *q, const void *elem);
```

Push a copy of `elem` and restore the heap property via sift-up. O(log n).

---

### `pqueue_pop`

```c
bool pqueue_pop(PQueue *q, void *out);
```

Remove and return the minimum element. Copies it into `*out` if `out` is not
`NULL`. Returns `true` on success, `false` if empty. O(log n).

```c
int v;
while (pqueue_pop(&q, &v))
    printf("%d\n", v);  // ascending order
```

---

### `pqueue_peek`

```c
void *pqueue_peek(const PQueue *q);
```

Return a pointer to the minimum element without removing it. Returns `NULL` if
empty. O(1).

---

### `pqueue_len` / `pqueue_is_empty`

```c
size_t pqueue_len(const PQueue *q);
int    pqueue_is_empty(const PQueue *q);
```

---

### `pqueue_clear`

```c
void pqueue_clear(PQueue *q);
```

Empty the priority queue. The underlying Vec buffer is retained.

---

### `pqueue_free`

```c
void pqueue_free(PQueue *q);
```

---

## Example: top-K elements

```c
Arena  *a = arena_create(4096);
PQueue  q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));

int data[] = {9, 3, 7, 1, 5, 8, 2, 6, 4, 0};
for (int i = 0; i < 10; i++)
    pqueue_push(&q, &data[i]);

// drain smallest 3
int v;
for (int i = 0; i < 3; i++) {
    pqueue_pop(&q, &v);
    printf("%d\n", v);  // 0, 1, 2
}

arena_free(a);
```

## Example: Dijkstra-style distance queue

```c
typedef struct { int dist; int node; } Entry;

static int entry_cmp(const void *a, const void *b) {
    return ((Entry *)a)->dist - ((Entry *)b)->dist;
}

PQueue q = pqueue_create(sizeof(Entry), entry_cmp, arena_allocator(a));

Entry e = {0, source};
pqueue_push(&q, &e);

while (!pqueue_is_empty(&q)) {
    pqueue_pop(&q, &e);
    // process e.node with distance e.dist
}
```
