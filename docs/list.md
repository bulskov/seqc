# list

Singly-linked list. Node data is stored inline (no separate allocation per
node). O(1) push/pop at the front, O(1) push at the back, O(n) pop from back.

**Header:** `include/seqc/list.h`  
**See also:** [`dlist`](dlist.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `list_node_t`

```c
typedef struct list_node_t list_node_t;
struct list_node_t {
  list_node_t *next;
  /* element data stored inline after the header, aligned to max_align_t */
};
```

### `list_t`

```c
typedef struct list_t list_t;
```

Opaque handle.

---

## Functions

### `list_create`

```c
list_t *list_create(size_t elem_size, allocator_t allocator);
```

Create an empty list. Returns `NULL` if `elem_size` is zero.

```c
Arena *a = arena_create(4096);
list_t  *l = list_create(sizeof(int), arena_allocator(a));
```

---

### `list_push_front` / `list_push_back`

```c
seqc_status_t list_push_front(list_t *l, const void *elem);
seqc_status_t list_push_back(list_t *l, const void *elem);
```

Prepend / append a copy of `elem`. Both are O(1). Return `SEQC_OOM` on
allocation failure; `SEQC_OK` otherwise.

---

### `list_pop_front` / `list_pop_back`

```c
seqc_status_t list_pop_front(list_t *l, void *out);
seqc_status_t list_pop_back(list_t *l, void *out);
```

Remove the front / back element. Copies it into `*out` if not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if empty.

`list_pop_front` is O(1). `list_pop_back` is O(n) — it must walk to the
second-to-last node. Use [`dlist`](dlist.md) if you need O(1) pop from
both ends.

---

### `list_front` / `list_back`

```c
void *list_front(const list_t *l);
void *list_back(const list_t *l);
```

Pointer to the head / tail element data. Returns `NULL` if empty.

---

### `list_is_empty` / `list_len`

```c
bool   list_is_empty(const list_t *l);
size_t list_len(const list_t *l);
```

---

### `list_iter`

```c
iter_t list_iter(const list_t *l);
```

Forward [`iter_t`](iter.md) from front to back.

---

### `list_clear`

```c
void list_clear(list_t *l);
```

Remove all nodes. Each node's memory is returned to the allocator (a no-op for
arena allocators). After clearing, `len == 0` and `head == tail == NULL`.

---

### `list_free`

```c
void list_free(list_t *l);
```

Free all nodes and then the list struct itself. Do not use `l` after calling this.

---

## Example

```c
Arena *a = arena_create(4096);
list_t  *l = list_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    list_push_back(l, &i);

iter_t it = list_iter(l);
int  v;
while (it.next(&it, &v))
    printf("%d ", v);  // 1 2 3 4 5
iter_drop(&it);

arena_free(a);
```
