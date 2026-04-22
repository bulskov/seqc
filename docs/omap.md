# omap

Ordered map backed by an [AVL tree](avl.md). Keys are kept sorted by the
supplied comparator. All operations are O(log n).

**Header:** `src/omap/omap.h`  
**See also:** [`avl`](avl.md) · [`hashmap`](hashmap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `OMapEntry`

```c
typedef struct {
  void *key;
  void *value;
} OMapEntry;
```

Yielded by the iterators. Both pointers point directly into live node storage —
do not modify the map while iterating.

### `OMap`

```c
typedef struct {
  OMNode    *root;
  size_t     len;
  size_t     key_size;
  size_t     val_size;
  compare_fn cmp;
  Allocator  allocator;
} OMap;
```

---

## Functions

### `omap_create`

```c
OMap omap_create(size_t key_size, size_t val_size,
                 compare_fn cmp, Allocator allocator);
```

Create an empty ordered map.

```c
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

Arena *a = arena_create(4096);
OMap   m = omap_create(sizeof(int), sizeof(double),
                        int_cmp, arena_allocator(a));
```

---

### `omap_set`

```c
bool omap_set(OMap *m, const void *key, const void *value);
```

Insert or update. Returns `true` if a new key was inserted, `false` if an existing
key was updated.

```c
int    k = 1;
double v = 3.14;
omap_set(&m, &k, &v);
```

---

### `omap_get`

```c
void *omap_get(const OMap *m, const void *key);
```

Return a pointer to the stored value, or `NULL` if not found.

```c
double *val = omap_get(&m, &(int){1});
if (val) printf("%.2f\n", *val);
```

---

### `omap_contains`

```c
bool omap_contains(const OMap *m, const void *key);
```

---

### `omap_remove`

```c
bool omap_remove(OMap *m, const void *key);
```

Remove by key. Returns `true` if removed, `false` if not found.

---

### `omap_min_key` / `omap_max_key`

```c
void *omap_min_key(const OMap *m);
void *omap_max_key(const OMap *m);
```

Pointer to the smallest / largest key. Returns `NULL` if empty.

---

### `omap_len` / `omap_height`

```c
size_t omap_len(const OMap *m);
int    omap_height(const OMap *m);   /* 0 if empty */
```

---

### `omap_iter`

```c
Iter omap_iter(const OMap *m);
```

Ascending key-order [`Iter`](iter.md). Each element is an [`OMapEntry`](#omapentry).

### `omap_iter_rev`

```c
Iter omap_iter_rev(const OMap *m);
```

Descending key-order [`Iter`](iter.md).

### `omap_iter_range`

```c
Iter omap_iter_range(const OMap *m, const void *lo_key, const void *hi_key);
```

Ascending key-order iteration over entries where `lo_key <= key <= hi_key`.
Pass `NULL` for either bound to leave it open.

```c
int lo = 10, hi = 20;
Iter      it = omap_iter_range(&m, &lo, &hi);
OMapEntry e;
while (it.next(&it, &e))
    printf("%d => %.2f\n", *(int *)e.key, *(double *)e.value);
iter_drop(&it);
```

---

### `omap_free`

```c
void omap_free(OMap *m);
```

---

## Example

```c
Arena *a = arena_create(4096);
OMap   m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));

// insert word counts
const char *words[] = {"apple", "banana", "cherry", "apple", "banana", "apple"};
// (using int keys for brevity)
for (int i = 1; i <= 5; i++) {
    int v = i * 100;
    omap_set(&m, &i, &v);
}

printf("min=%d max=%d\n",
       *(int *)omap_min_key(&m),  // 1
       *(int *)omap_max_key(&m)); // 5

// iterate all in ascending order
Iter      it = omap_iter(&m);
OMapEntry e;
while (it.next(&it, &e))
    printf("%d => %d\n", *(int *)e.key, *(int *)e.value);
iter_drop(&it);

arena_free(a);
```
