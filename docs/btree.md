# btree

Unbalanced binary search tree. O(log n) average for insert, lookup, and remove;
O(n) worst case on sorted input. Use [`avl`](avl.md) when balance guarantees
matter.

**Header:** `src/btree/btree.h`  
**See also:** [`avl`](avl.md) · [`omap`](omap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `BTree`

```c
typedef struct BTree BTree;
```

Opaque handle.

---

## Functions

### `btree_create`

```c
BTree *btree_create(size_t elem_size, compare_fn cmp, Allocator allocator);
```

Create an empty tree. Returns `NULL` if `elem_size` is zero. `cmp` follows the
[`compare_fn`](iter.md#function-pointer-types) convention: negative / zero /
positive.

```c
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

Arena *a = arena_create(4096);
BTree *t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
```

---

### `btree_insert`

```c
bool btree_insert(BTree *t, const void *elem);
```

Insert a copy of `elem`. Returns `true` if inserted, `false` if a duplicate already
exists (duplicates are rejected).

---

### `btree_contains`

```c
bool btree_contains(const BTree *t, const void *elem);
```

Return `true` if `elem` is present.

---

### `btree_remove`

```c
bool btree_remove(BTree *t, const void *elem);
```

Remove `elem`. Returns `true` if removed, `false` if not found.

---

### `btree_min` / `btree_max`

```c
void *btree_min(const BTree *t);
void *btree_max(const BTree *t);
```

Pointer to the minimum / maximum element. Returns `NULL` if empty.

---

### `btree_len`

```c
size_t btree_len(const BTree *t);
```

---

### `btree_height`

```c
int btree_height(const BTree *t);
```

Return the height of the tree (longest root-to-leaf path). Returns `0` for an
empty tree, `1` for a single-node tree. Because `BTree` is unbalanced,
height can be as large as `n` on sorted input.

```c
BTree *t = btree_create(sizeof(int), int_cmp, arena_allocator(a));
int vals[] = {5, 3, 7, 1, 4, 6, 8};
for (int i = 0; i < 7; i++)
    btree_insert(t, &vals[i]);
printf("height=%d\n", btree_height(t));  // 3
```

### `btree_iter`

```c
Iter btree_iter(const BTree *t);
```

Ascending in-order [`Iter`](iter.md).

### `btree_iter_rev`

```c
Iter btree_iter_rev(const BTree *t);
```

Descending in-order [`Iter`](iter.md).

### `btree_iter_range`

```c
Iter btree_iter_range(const BTree *t, const void *lo, const void *hi);
```

Ascending in-order iteration over elements where `lo <= elem <= hi`.
Pass `NULL` for `lo` or `hi` to leave that bound open.

```c
int lo = 3, hi = 7;
Iter it = btree_iter_range(t, &lo, &hi);
int  v;
while (it.next(&it, &v))
    printf("%d ", v);  // 3 4 5 6 7
iter_drop(&it);
```

---

### `btree_clear`

```c
void btree_clear(BTree *t);
```

Remove all elements. Every node is freed back to the allocator (a no-op for
arena allocators) and `root` is set to NULL. The `BTree` struct remains valid
and can be reused immediately.

---

### `btree_free`

```c
void btree_free(BTree *t);
```

Free all nodes and then the BTree struct itself. Do not use `t` after calling this.

---

## Example

```c
Arena *a = arena_create(4096);
BTree *t = btree_create(sizeof(int), int_cmp, arena_allocator(a));

int vals[] = {5, 3, 7, 1, 4, 6, 8};
for (int i = 0; i < 7; i++)
    btree_insert(t, &vals[i]);

printf("min=%d max=%d len=%zu\n",
       *(int *)btree_min(t),   // 1
       *(int *)btree_max(t),   // 8
       btree_len(t));           // 7

// ascending
Iter it = btree_iter(t);
int v;
while (it.next(&it, &v)) printf("%d ", v);
iter_drop(&it);
// 1 3 4 5 6 7 8

arena_free(a);
```
