# list

Singly-linked list. Node data is stored inline (no separate allocation per
node). O(1) push/pop at the front, O(1) push at the back, O(n) pop from back.

**Header:** `include/seqc/list.h`  
**See also:** [`dlist`](dlist.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `ListNode`

```c
typedef struct ListNode ListNode;
struct ListNode {
  ListNode *next;
  /* element data stored inline after the header, aligned to max_align_t */
};
```

### `List`

```c
typedef struct List List;
```

Opaque handle.

---

## Functions

### `list_create`

```c
List *list_create(size_t elem_size, Allocator allocator);
```

Create an empty list. Returns `NULL` if `elem_size` is zero.

```c
Arena *a = arena_create(4096);
List  *l = list_create(sizeof(int), arena_allocator(a));
```

---

### `list_push_front` / `list_push_back`

```c
SeqcStatus list_push_front(List *l, const void *elem);
SeqcStatus list_push_back(List *l, const void *elem);
```

Prepend / append a copy of `elem`. Both are O(1). Return `SEQC_OOM` on
allocation failure; `SEQC_OK` otherwise.

---

### `list_pop_front` / `list_pop_back`

```c
SeqcStatus list_pop_front(List *l, void *out);
SeqcStatus list_pop_back(List *l, void *out);
```

Remove the front / back element. Copies it into `*out` if not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if empty.

`list_pop_front` is O(1). `list_pop_back` is O(n) — it must walk to the
second-to-last node. Use [`dlist`](dlist.md) if you need O(1) pop from
both ends.

---

### `list_front` / `list_back`

```c
void *list_front(const List *l);
void *list_back(const List *l);
```

Pointer to the head / tail element data. Returns `NULL` if empty.

---

### `list_is_empty` / `list_len`

```c
bool   list_is_empty(const List *l);
size_t list_len(const List *l);
```

---

### `list_iter`

```c
Iter list_iter(const List *l);
```

Forward [`Iter`](iter.md) from front to back.

---

### `list_clear`

```c
void list_clear(List *l);
```

Remove all nodes. Each node's memory is returned to the allocator (a no-op for
arena allocators). After clearing, `len == 0` and `head == tail == NULL`.

---

### `list_free`

```c
void list_free(List *l);
```

Free all nodes and then the List struct itself. Do not use `l` after calling this.

---

## Example

```c
Arena *a = arena_create(4096);
List  *l = list_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    list_push_back(l, &i);

Iter it = list_iter(l);
int  v;
while (it.next(&it, &v))
    printf("%d ", v);  // 1 2 3 4 5
iter_drop(&it);

arena_free(a);
```
