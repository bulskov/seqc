# avl

Self-balancing AVL tree. Guaranteed O(log n) for insert, remove, and lookup.
The height invariant `|height(L) - height(R)| <= 1` is maintained via LL / RR /
LR / RL rotations.

**Header:** `include/seqc/avl.h`  
**See also:** [`bstree`](bstree.md) · [`omap`](omap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `AVLTree`

```c
typedef struct AVLTree AVLTree;
```

Opaque handle.

---

## Functions

### `avl_create`

```c
AVLTree *avl_create(size_t elem_size, compare_fn cmp, Allocator allocator);
```

Create an empty tree. Returns `NULL` if `elem_size` is zero.

```c
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

Arena   *a = arena_create(4096);
AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));
```

---

### `avl_insert`

```c
SeqcStatus avl_insert(AVLTree *t, const void *elem);
```

Insert a copy of `elem`. Returns `SEQC_OK` if inserted, `SEQC_DUPLICATE` if
already present (tree unchanged), `SEQC_OOM` on allocation failure.

---

### `avl_contains`

```c
bool avl_contains(const AVLTree *t, const void *elem);
```

---

### `avl_remove`

```c
SeqcStatus avl_remove(AVLTree *t, const void *elem);
```

Remove `elem`. Returns `SEQC_OK` if removed, `SEQC_NOT_FOUND` if absent.

---

### `avl_min` / `avl_max`

```c
void *avl_min(const AVLTree *t);
void *avl_max(const AVLTree *t);
```

Pointer to the minimum / maximum element. Returns `NULL` if empty.

---

### `avl_len` / `avl_height`

```c
size_t avl_len(const AVLTree *t);
int    avl_height(const AVLTree *t);   /* 0 if empty */
```

The height is bounded by `1.44 * log2(n + 2)` due to the AVL invariant.

---

### `avl_iter`

```c
Iter avl_iter(const AVLTree *t);
```

Ascending in-order [`Iter`](iter.md).

### `avl_iter_rev`

```c
Iter avl_iter_rev(const AVLTree *t);
```

Descending in-order [`Iter`](iter.md).

### `avl_iter_range`

```c
Iter avl_iter_range(const AVLTree *t, const void *lo, const void *hi);
```

Ascending in-order iteration over elements where `lo <= elem <= hi`.
Pass `NULL` for `lo` or `hi` for an open bound. Descends to the first
in-range element in O(log n).

```c
int lo = 10, hi = 50;
Iter it = avl_iter_range(t, &lo, &hi);
int  v;
while (it.next(&it, &v))
    printf("%d ", v);
iter_drop(&it);
```

---

### `avl_clear`

```c
void avl_clear(AVLTree *t);
```

Remove all elements. Every node is freed back to the allocator (a no-op for
arena allocators) and `root` is set to NULL. The `AVLTree` struct remains valid
and can be reused immediately.

---

### `avl_free`

```c
void avl_free(AVLTree *t);
```

Free all nodes and then the AVLTree struct itself. Do not use `t` after calling this.

---

## Example

```c
Arena   *a = arena_create(4096);
AVLTree *t = avl_create(sizeof(int), int_cmp, arena_allocator(a));

for (int i = 1; i <= 1000; i++)
    avl_insert(t, &i);

printf("height=%d (log2(1000)~10)\n", avl_height(t));

avl_remove(t, &(int){500});
printf("len=%zu contains(500)=%d\n",
       avl_len(t),
       avl_contains(t, &(int){500}));  // 0

arena_free(a);
```
