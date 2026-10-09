# avl

Self-balancing AVL tree. Guaranteed O(log n) for insert, remove, and lookup.
The height invariant `|height(L) - height(R)| <= 1` is maintained via LL / RR /
LR / RL rotations.

**Header:** `include/seqc/avl.h`  
**See also:** [`bstree`](bstree.md) · [`omap`](omap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `avl_t`

```c
typedef struct avl_t avl_t;
```

Opaque handle.

---

## Functions

### `avl_create`

```c
avl_t *avl_create(size_t elem_size, compare_fn cmp, allocator_t allocator);
```

Create an empty tree. Returns `NULL` if `elem_size` is zero.

```c
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

growing_arena_t arena;
growing_arena_init(&arena, 4096);
allocator_t a = growing_arena_allocator(&arena);
avl_t *t = avl_create(sizeof(int), int_cmp, a);
```

---

### `avl_add`

```c
seqc_status_t avl_add(avl_t *t, const void *elem);
```

Insert a copy of `elem`. Returns `SEQC_OK` if inserted, `SEQC_DUPLICATE` if
already present (tree unchanged), `SEQC_OOM` on allocation failure.

---

### `avl_contains`

```c
bool avl_contains(const avl_t *t, const void *elem);
```

---

### `avl_remove`

```c
seqc_status_t avl_remove(avl_t *t, const void *elem);
```

Remove `elem`. Returns `SEQC_OK` if removed, `SEQC_NOT_FOUND` if absent.

---

### `avl_min_ptr` / `avl_max_ptr`

```c
void *avl_min_ptr(const avl_t *t);
void *avl_max_ptr(const avl_t *t);
```

Pointer to the minimum / maximum element. Returns `NULL` if empty.

---

### `avl_len` / `avl_height`

```c
size_t avl_len(const avl_t *t);
int    avl_height(const avl_t *t);   /* 0 if empty */
```

The height is bounded by `1.44 * log2(n + 2)` due to the AVL invariant.

---

### `avl_iter`

```c
iter_t avl_iter(const avl_t *t);
```

Ascending in-order [`iter_t`](iter.md).

### `avl_iter_rev`

```c
iter_t avl_iter_rev(const avl_t *t);
```

Descending in-order [`iter_t`](iter.md).

### `avl_iter_range`

```c
iter_t avl_iter_range(const avl_t *t, const void *lo, const void *hi);
```

Ascending in-order iteration over elements where `lo <= elem <= hi`.
Pass `NULL` for `lo` or `hi` for an open bound. Descends to the first
in-range element in O(log n).

```c
int lo = 10, hi = 50;
iter_t it = avl_iter_range(t, &lo, &hi);
int  v;
while (it.next(&it, &v))
    printf("%d ", v);
iter_destroy(&it);
```

---

### `avl_clear`

```c
void avl_clear(avl_t *t);
```

Remove all elements. Every node is freed back to the allocator (a no-op for
arena allocators) and `root` is set to NULL. The `avl_t` struct remains valid
and can be reused immediately.

---

### `avl_destroy`

```c
void avl_destroy(avl_t *t);
```

Free all nodes and then the avl_t struct itself. Do not use `t` after calling this.

---

### `avl_min` / `avl_max`

```c
seqc_status_t avl_min(const avl_t *t, void *out);
seqc_status_t avl_max(const avl_t *t, void *out);
```

Copy the smallest / largest element into `out`; `SEQC_NOT_FOUND` if empty. The pairs of `avl_min_ptr` / `avl_max_ptr`. See [naming](naming.md#reading-elements-copy-or-pointer): the copy is yours, the pointer is valid only until the next change.

---

### `avl_is_empty`

```c
bool avl_is_empty(const avl_t *t);
```

`true` if the tree has no elements, or is `NULL`.

---

### `avl_extend`

```c
seqc_status_t avl_extend(avl_t *t, iter_t it);
```

Add every element of `it`; duplicates are skipped, not errors. Add every element of `it`, which is always consumed — also on error and for a `NULL` collection (`SEQC_INVALID`). Stops at the first error and returns it.

---

## Example

```c
growing_arena_t arena;
growing_arena_init(&arena, 4096);
allocator_t a = growing_arena_allocator(&arena);
avl_t *t = avl_create(sizeof(int), int_cmp, a);

for (int i = 1; i <= 1000; i++)
    avl_add(t, &i);

printf("height=%d (log2(1000)~10)\n", avl_height(t));

avl_remove(t, &(int){500});
printf("len=%zu contains(500)=%d\n",
       avl_len(t),
       avl_contains(t, &(int){500}));  // 0

growing_arena_destroy(&arena);
```
