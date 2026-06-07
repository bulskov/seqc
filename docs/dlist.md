# dlist

Doubly-linked list. O(1) push and pop at both ends.

**Header:** `include/seqc/dlist.h`  
**See also:** [`list`](list.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `dlist_node_t` / `dlist_t`

```c
typedef struct dlist_node_t dlist_node_t;
typedef struct dlist_t dlist_t;
```

Both are opaque handles. Element data is stored inline immediately after each
node header, aligned to `max_align_t`.

---

## Functions

### `dlist_create`

```c
dlist_t *dlist_create(size_t elem_size, allocator_t allocator);
```

Create an empty doubly-linked list. Returns `NULL` if `elem_size` is zero.

```c
Arena *a = arena_create(4096);
dlist_t *l = dlist_create(sizeof(int), arena_allocator(a));
```

---

### `dlist_push_front` / `dlist_push_back`

```c
seqc_status_t dlist_push_front(dlist_t *l, const void *elem);
seqc_status_t dlist_push_back(dlist_t *l, const void *elem);
```

O(1) prepend / append. Return `SEQC_OOM` on allocation failure; `SEQC_OK` otherwise.

---

### `dlist_pop_front` / `dlist_pop_back`

```c
seqc_status_t dlist_pop_front(dlist_t *l, void *out);
seqc_status_t dlist_pop_back(dlist_t *l, void *out);
```

Remove from front or back. Copies into `*out` if not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if empty.

```c
// use as a deque
dlist_push_back(l, &val);
dlist_pop_front(l, &out);
```

---

### `dlist_front` / `dlist_back`

```c
void *dlist_front(const dlist_t *l);
void *dlist_back(const dlist_t *l);
```

Pointer to head / tail data. Returns `NULL` if empty.

---

### `dlist_is_empty` / `dlist_len`

```c
bool   dlist_is_empty(const dlist_t *l);
size_t dlist_len(const dlist_t *l);
```

---

### `dlist_iter`

```c
iter_t dlist_iter(const dlist_t *l);
```

Forward [`iter_t`](iter.md) from front to back.

### `dlist_iter_rev`

```c
iter_t dlist_iter_rev(const dlist_t *l);
```

Reverse [`iter_t`](iter.md) from back to front.

---

### `dlist_clear`

```c
void dlist_clear(dlist_t *l);
```

Remove all nodes. Each node's memory is returned to the allocator (a no-op for
arena allocators). After clearing, `len == 0` and `head == tail == NULL`.

---

### `dlist_free`

```c
void dlist_free(dlist_t *l);
```

Free all nodes and then the dlist_t struct itself. Do not use `l` after calling this.

---

## Example

```c
Arena *a = arena_create(4096);
dlist_t *l = dlist_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    dlist_push_back(l, &i);

// forward: 1 2 3 4 5
iter_t fwd = dlist_iter(l);
int v;
while (fwd.next(&fwd, &v)) printf("%d ", v);
iter_drop(&fwd);

// reverse: 5 4 3 2 1
iter_t rev = dlist_iter_rev(l);
while (rev.next(&rev, &v)) printf("%d ", v);
iter_drop(&rev);

arena_free(a);
```
