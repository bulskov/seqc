# pqueue

Binary min-heap priority queue. The element with the lowest
[`compare_fn`](iter.md#function-pointer-types) value is always at the front.
All push/pop operations are O(log n); peek is O(1).

**Header:** `include/seqc/pqueue.h`  
**See also:** [`vec`](vec.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Type

### `pqueue_t`

```c
typedef struct pqueue_t pqueue_t;
```

Opaque handle. Backed internally by a [`vec_t`](vec.md); elements are stored
contiguously and the heap property is maintained by sift-up on push and
sift-down on pop.

---

## Functions

### `pqueue_create`

```c
pqueue_t *pqueue_create(size_t elem_size, compare_fn cmp, allocator_t allocator);
```

Create an empty priority queue. Returns `NULL` if `elem_size` is zero.

```c
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

Arena  *a = arena_create(4096);
pqueue_t *q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));
```

To get a **max-heap**, pass a negated comparator:

```c
static int int_cmp_desc(const void *a, const void *b) {
    return *(const int *)b - *(const int *)a;
}
pqueue_t *max_heap = pqueue_create(sizeof(int), int_cmp_desc, arena_allocator(a));
```

---

### `pqueue_build_from_vec`

```c
pqueue_t *pqueue_build_from_vec(const vec_t *v, compare_fn cmp, allocator_t allocator);
```

Build a priority queue from a copy of `v`'s elements using Floyd's O(n)
bottom-up heapify algorithm. This is faster than pushing elements one by one
(O(n log n)) when the full data set is known upfront. The source `vec_t` is not
modified. The returned `pqueue_t` owns its own allocation independent of `v`.

```c
Arena  *a = arena_create(4096);
vec_t    *v = vec_create(sizeof(int), arena_allocator(a));
int data[] = {9, 3, 7, 1, 5, 8, 2, 6, 4, 0};
for (int i = 0; i < 10; i++)
    vec_push(v, &data[i]);

pqueue_t *q = pqueue_build_from_vec(v, int_cmp, arena_allocator(a));
// Identical to pushing all elements one-by-one but O(n) instead of O(n log n)

int v_out;
while (pqueue_pop(q, &v_out) == SEQC_OK)
    printf("%d\n", v_out);  // 0 1 2 3 4 5 6 7 8 9
arena_free(a);
```

---

### `pqueue_push`

```c
seqc_status_t pqueue_push(pqueue_t *q, const void *elem);
```

Push a copy of `elem` and restore the heap property via sift-up. Returns
`SEQC_OOM` on allocation failure; `SEQC_OK` otherwise. O(log n).

---

### `pqueue_pop`

```c
seqc_status_t pqueue_pop(pqueue_t *q, void *out);
```

Remove and return the minimum element. Copies it into `*out` if `out` is not
`NULL`. Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if empty. O(log n).

```c
int v;
while (pqueue_pop(q, &v) == SEQC_OK)
    printf("%d\n", v);  // ascending order
```

---

### `pqueue_peek`

```c
seqc_status_t pqueue_peek(const pqueue_t *q, void *out);
```

Copy the minimum element into `*out` without removing it. `out` may be `NULL`
to test for non-emptiness. Returns `SEQC_OK` if the queue is non-empty,
`SEQC_NOT_FOUND` otherwise. O(1).

```c
int top;
if (pqueue_peek(q, &top) == SEQC_OK)
    printf("min: %d\n", top);
```

---

### `pqueue_len` / `pqueue_is_empty`

```c
size_t pqueue_len(const pqueue_t *q);
bool   pqueue_is_empty(const pqueue_t *q);
```

---

### `pqueue_iter`

```c
iter_t pqueue_iter(const pqueue_t *q);
```

Iterate over elements in **heap-storage order** (not priority order). Useful
for inspection or bulk processing. The queue is not modified. To consume
elements in priority order, use `pqueue_pop` in a loop.

---

### `pqueue_iter_rev`

```c
iter_t pqueue_iter_rev(const pqueue_t *q);
```

Iterate in **reverse heap-storage order**. Equivalent to reversing the
sequence produced by `pqueue_iter`.

---

### `pqueue_clear`

```c
void pqueue_clear(pqueue_t *q);
```

Empty the priority queue. The underlying vec_t buffer is retained.

---

### `pqueue_drain`

```c
slice_t pqueue_drain(pqueue_t *q, allocator_t allocator);
```

Pop all elements in priority order into an `allocator`-owned [`slice_t`](slice.md).
The queue is empty after this call but is not freed and may be reused.
Returns an empty slice_t (NULL ptr, len 0) if the queue is empty or allocation
fails.

```c
/* drain and iterate in sorted order */
slice_t sorted = pqueue_drain(q, arena_allocator(a));
for (size_t i = 0; i < sorted.len; i++)
    printf("%d\n", *(int *)slice_get(sorted, i));
```

---

### `pqueue_free`

```c
void pqueue_free(pqueue_t *q);
```

Free the pqueue_t and all its internal storage. Do not use `q` after calling this.

---

## Example: top-K elements

```c
Arena  *a = arena_create(4096);
pqueue_t *q = pqueue_create(sizeof(int), int_cmp, arena_allocator(a));

int data[] = {9, 3, 7, 1, 5, 8, 2, 6, 4, 0};
for (int i = 0; i < 10; i++)
    pqueue_push(q, &data[i]);

// drain smallest 3
int v;
for (int i = 0; i < 3; i++) {
    pqueue_pop(q, &v);
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

pqueue_t *q = pqueue_create(sizeof(Entry), entry_cmp, arena_allocator(a));

Entry e = {0, source};
pqueue_push(q, &e);

while (!pqueue_is_empty(q)) {
    pqueue_pop(q, &e);
    // process e.node with distance e.dist
}
```
