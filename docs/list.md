# list

Singly-linked list. Node data is stored inline (no separate allocation per
node). O(1) push/pop at the front, O(1) push at the back, O(n) pop from back.

**Header:** `src/list/list.h`  
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
typedef struct {
  ListNode *head;
  ListNode *tail;
  size_t    len;
  size_t    elem_size;
  Allocator allocator;
} List;
```

---

## Functions

### `list_create`

```c
List list_create(size_t elem_size, Allocator allocator);
```

Create an empty list.

```c
Arena *a = arena_create(4096);
List   l = list_create(sizeof(int), arena_allocator(a));
```

---

### `list_push_front` / `list_push_back`

```c
void list_push_front(List *l, const void *elem);
void list_push_back(List *l, const void *elem);
```

Prepend / append a copy of `elem`. Both are O(1).

---

### `list_pop_front`

```c
bool list_pop_front(List *l, void *out);
```

Remove the front element. Copies it into `*out` if not `NULL`.
Returns `true` on success, `false` if empty.

> **Note:** Popping from the back is O(n) on a singly-linked list and not
> provided. Use [`dlist`](dlist.md) if you need O(1) pop from both ends.

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
int    list_is_empty(const List *l);
size_t list_len(const List *l);
```

---

### `list_iter`

```c
Iter list_iter(const List *l);
```

Forward [`Iter`](iter.md) from front to back.

---

### `list_free`

```c
void list_free(List *l);
```

---

## Example

```c
Arena *a = arena_create(4096);
List   l = list_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    list_push_back(&l, &i);

Iter it = list_iter(&l);
int  v;
while (it.next(&it, &v))
    printf("%d ", v);  // 1 2 3 4 5
iter_drop(&it);

arena_free(a);
```
