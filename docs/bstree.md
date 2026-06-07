# bstree

Unbalanced binary search tree. O(log n) average for insert, lookup, and remove;
O(n) worst case on sorted input. Use [`avl`](avl.md) when balance guarantees
matter.

**Header:** `include/seqc/bstree.h`  
**See also:** [`avl`](avl.md) · [`omap`](omap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `bstree_t`

```c
typedef struct bstree_t bstree_t;
```

Opaque handle.

---

## Functions

### `bstree_create`

```c
bstree_t *bstree_create(size_t elem_size, compare_fn cmp, allocator_t allocator);
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
bstree_t *t = bstree_create(sizeof(int), int_cmp, arena_allocator(a));
```

---

### `bstree_insert`

```c
seqc_status_t bstree_insert(bstree_t *t, const void *elem);
```

Insert a copy of `elem`. Returns `SEQC_OK` if inserted, `SEQC_DUPLICATE` if
already present (tree unchanged), `SEQC_OOM` on allocation failure.

---

### `bstree_contains`

```c
bool bstree_contains(const bstree_t *t, const void *elem);
```

Return `true` if `elem` is present.

---

### `bstree_remove`

```c
seqc_status_t bstree_remove(bstree_t *t, const void *elem);
```

Remove `elem`. Returns `SEQC_OK` if removed, `SEQC_NOT_FOUND` if absent.

---

### `bstree_min` / `bstree_max`

```c
void *bstree_min(const bstree_t *t);
void *bstree_max(const bstree_t *t);
```

Pointer to the minimum / maximum element. Returns `NULL` if empty.

---

### `bstree_len`

```c
size_t bstree_len(const bstree_t *t);
```

---

### `bstree_height`

```c
int bstree_height(const bstree_t *t);
```

Return the height of the tree (longest root-to-leaf path). Returns `0` for an
empty tree, `1` for a single-node tree. Because `bstree_t` is unbalanced,
height can be as large as `n` on sorted input.

```c
bstree_t *t = bstree_create(sizeof(int), int_cmp, arena_allocator(a));
int vals[] = {5, 3, 7, 1, 4, 6, 8};
for (int i = 0; i < 7; i++)
    bstree_insert(t, &vals[i]);
printf("height=%d\n", bstree_height(t));  // 3
```

### `bstree_iter`

```c
iter_t bstree_iter(const bstree_t *t);
```

Ascending in-order [`iter_t`](iter.md).

### `bstree_iter_rev`

```c
iter_t bstree_iter_rev(const bstree_t *t);
```

Descending in-order [`iter_t`](iter.md).

### `bstree_iter_range`

```c
iter_t bstree_iter_range(const bstree_t *t, const void *lo, const void *hi);
```

Ascending in-order iteration over elements where `lo <= elem <= hi`.
Pass `NULL` for `lo` or `hi` to leave that bound open.

```c
int lo = 3, hi = 7;
iter_t it = bstree_iter_range(t, &lo, &hi);
int  v;
while (it.next(&it, &v))
    printf("%d ", v);  // 3 4 5 6 7
iter_drop(&it);
```

---

### `bstree_clear`

```c
void bstree_clear(bstree_t *t);
```

Remove all elements. Every node is freed back to the allocator (a no-op for
arena allocators) and `root` is set to NULL. The `bstree_t` struct remains valid
and can be reused immediately.

---

### `bstree_free`

```c
void bstree_free(bstree_t *t);
```

Free all nodes and then the bstree_t struct itself. Do not use `t` after calling this.

---

## Example

```c
Arena  *a = arena_create(4096);
bstree_t *t = bstree_create(sizeof(int), int_cmp, arena_allocator(a));

int vals[] = {5, 3, 7, 1, 4, 6, 8};
for (int i = 0; i < 7; i++)
    bstree_insert(t, &vals[i]);

printf("min=%d max=%d len=%zu\n",
       *(int *)bstree_min(t),   // 1
       *(int *)bstree_max(t),   // 8
       bstree_len(t));           // 7

// ascending
iter_t it = bstree_iter(t);
int v;
while (it.next(&it, &v)) printf("%d ", v);
iter_drop(&it);
// 1 3 4 5 6 7 8

arena_free(a);
```
