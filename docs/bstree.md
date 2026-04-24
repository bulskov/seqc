# bstree

Unbalanced binary search tree. O(log n) average for insert, lookup, and remove;
O(n) worst case on sorted input. Use [`avl`](avl.md) when balance guarantees
matter.

**Header:** `include/seqc/bstree.h`  
**See also:** [`avl`](avl.md) · [`omap`](omap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `BSTree`

```c
typedef struct BSTree BSTree;
```

Opaque handle.

---

## Functions

### `bstree_create`

```c
BSTree *bstree_create(size_t elem_size, compare_fn cmp, Allocator allocator);
```

Create an empty tree. Returns `NULL` if `elem_size` is zero. `cmp` follows the
[`compare_fn`](iter.md#function-pointer-types) convention: negative / zero /
positive.

```c
static int int_cmp(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

Arena  *a = arena_create(4096);
BSTree *t = bstree_create(sizeof(int), int_cmp, arena_allocator(a));
```

---

### `bstree_insert`

```c
SeqcStatus bstree_insert(BSTree *t, const void *elem);
```

Insert a copy of `elem`. Returns `SEQC_OK` if inserted, `SEQC_DUPLICATE` if
already present (tree unchanged), `SEQC_OOM` on allocation failure.

---

### `bstree_contains`

```c
bool bstree_contains(const BSTree *t, const void *elem);
```

Return `true` if `elem` is present.

---

### `bstree_remove`

```c
SeqcStatus bstree_remove(BSTree *t, const void *elem);
```

Remove `elem`. Returns `SEQC_OK` if removed, `SEQC_NOT_FOUND` if absent.

---

### `bstree_min` / `bstree_max`

```c
void *bstree_min(const BSTree *t);
void *bstree_max(const BSTree *t);
```

Pointer to the minimum / maximum element. Returns `NULL` if empty.

---

### `bstree_len`

```c
size_t bstree_len(const BSTree *t);
```

---

### `bstree_height`

```c
int bstree_height(const BSTree *t);
```

Return the height of the tree (longest root-to-leaf path). Returns `0` for an
empty tree, `1` for a single-node tree. Because `BSTree` is unbalanced,
height can be as large as `n` on sorted input.

```c
BSTree *t = bstree_create(sizeof(int), int_cmp, arena_allocator(a));
int vals[] = {5, 3, 7, 1, 4, 6, 8};
for (int i = 0; i < 7; i++)
    bstree_insert(t, &vals[i]);
printf("height=%d\n", bstree_height(t));  // 3
```

### `bstree_iter`

```c
Iter bstree_iter(const BSTree *t);
```

Ascending in-order [`Iter`](iter.md).

### `bstree_iter_rev`

```c
Iter bstree_iter_rev(const BSTree *t);
```

Descending in-order [`Iter`](iter.md).

### `bstree_iter_range`

```c
Iter bstree_iter_range(const BSTree *t, const void *lo, const void *hi);
```

Ascending in-order iteration over elements where `lo <= elem <= hi`.
Pass `NULL` for `lo` or `hi` to leave that bound open.

```c
int lo = 3, hi = 7;
Iter it = bstree_iter_range(t, &lo, &hi);
int  v;
while (it.next(&it, &v))
    printf("%d ", v);  // 3 4 5 6 7
iter_drop(&it);
```

---

### `bstree_clear`

```c
void bstree_clear(BSTree *t);
```

Remove all elements. Every node is freed back to the allocator (a no-op for
arena allocators) and `root` is set to NULL. The `BSTree` struct remains valid
and can be reused immediately.

---

### `bstree_free`

```c
void bstree_free(BSTree *t);
```

Free all nodes and then the BSTree struct itself. Do not use `t` after calling this.

---

## Example

```c
Arena  *a = arena_create(4096);
BSTree *t = bstree_create(sizeof(int), int_cmp, arena_allocator(a));

int vals[] = {5, 3, 7, 1, 4, 6, 8};
for (int i = 0; i < 7; i++)
    bstree_insert(t, &vals[i]);

printf("min=%d max=%d len=%zu\n",
       *(int *)bstree_min(t),   // 1
       *(int *)bstree_max(t),   // 8
       bstree_len(t));           // 7

// ascending
Iter it = bstree_iter(t);
int v;
while (it.next(&it, &v)) printf("%d ", v);
iter_drop(&it);
// 1 3 4 5 6 7 8

arena_free(a);
```
