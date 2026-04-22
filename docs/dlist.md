# dlist

Doubly-linked list. O(1) push and pop at both ends.

**Header:** `src/dlist/dlist.h`  
**See also:** [`list`](list.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `DListNode`

```c
typedef struct DListNode DListNode;
struct DListNode {
  DListNode *prev;
  DListNode *next;
  /* element data stored inline after the header, aligned to max_align_t */
};
```

### `DList`

```c
typedef struct {
  DListNode *head;
  DListNode *tail;
  size_t     len;
  size_t     elem_size;
  Allocator  allocator;
} DList;
```

---

## Functions

### `dlist_create`

```c
DList dlist_create(size_t elem_size, Allocator allocator);
```

Create an empty doubly-linked list.

```c
Arena *a = arena_create(4096);
DList  l = dlist_create(sizeof(int), arena_allocator(a));
```

---

### `dlist_push_front` / `dlist_push_back`

```c
void dlist_push_front(DList *l, const void *elem);
void dlist_push_back(DList *l, const void *elem);
```

O(1) prepend / append.

---

### `dlist_pop_front` / `dlist_pop_back`

```c
bool dlist_pop_front(DList *l, void *out);
bool dlist_pop_back(DList *l, void *out);
```

Remove from front or back. Copies into `*out` if not `NULL`.
Returns `true` on success, `false` if empty.

```c
// use as a deque
dlist_push_back(&l, &val);
dlist_pop_front(&l, &out);
```

---

### `dlist_front` / `dlist_back`

```c
void *dlist_front(const DList *l);
void *dlist_back(const DList *l);
```

Pointer to head / tail data. Returns `NULL` if empty.

---

### `dlist_is_empty` / `dlist_len`

```c
int    dlist_is_empty(const DList *l);
size_t dlist_len(const DList *l);
```

---

### `dlist_iter`

```c
Iter dlist_iter(const DList *l);
```

Forward [`Iter`](iter.md) from front to back.

### `dlist_iter_reverse`

```c
Iter dlist_iter_reverse(const DList *l);
```

Reverse [`Iter`](iter.md) from back to front.

---

### `dlist_free`

```c
void dlist_free(DList *l);
```

---

## Example

```c
Arena *a = arena_create(4096);
DList  l = dlist_create(sizeof(int), arena_allocator(a));

for (int i = 1; i <= 5; i++)
    dlist_push_back(&l, &i);

// forward: 1 2 3 4 5
Iter fwd = dlist_iter(&l);
int v;
while (fwd.next(&fwd, &v)) printf("%d ", v);
iter_drop(&fwd);

// reverse: 5 4 3 2 1
Iter rev = dlist_iter_reverse(&l);
while (rev.next(&rev, &v)) printf("%d ", v);
iter_drop(&rev);

arena_free(a);
```
