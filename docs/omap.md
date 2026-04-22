# omap

Ordered map backed by an [AVL tree](avl.md). Keys are kept sorted by the
supplied comparator. All operations are O(log n).

**Header:** `src/omap/omap.h`  
**See also:** [`avl`](avl.md) · [`hashmap`](hashmap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `OMapEntry`

```c
typedef MapEntry OMapEntry;
```

Alias for [`MapEntry`](iter.md#mapentry) (defined in `iter/iter.h`).
Yielded by the iterators. Both pointers point directly into live node storage —
do not modify the map while iterating.

### `OMap`

```c
typedef struct OMap OMap;
```

Opaque handle.

---

## Functions

### `omap_create`

```c
OMap *omap_create(size_t key_size, size_t val_size,
                  compare_fn cmp, Allocator allocator);
```

Create an empty ordered map. Returns `NULL` if `key_size` or `val_size` is zero.

```c
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

Arena *a = arena_create(4096);
OMap  *m = omap_create(sizeof(int), sizeof(double),
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
omap_set(m, &k, &v);
```

---

### `omap_get`

```c
bool omap_get(const OMap *m, const void *key, void *out);
```

Copy the value for `key` into `*out`. `out` may be `NULL` to test for
presence only. Returns `true` if found, `false` otherwise.

```c
double val;
if (omap_get(m, &(int){1}, &val))
    printf("%.2f\n", val);
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
bool omap_min_key(const OMap *m, void *out);
bool omap_max_key(const OMap *m, void *out);
```

Copy the smallest / largest key into `*out`. Returns `false` if the map is
empty; `out` may be `NULL` to test for non-emptiness.

---

### `omap_min_entry` / `omap_max_entry`

```c
bool omap_min_entry(const OMap *m, void *key_out, void *val_out);
bool omap_max_entry(const OMap *m, void *key_out, void *val_out);
```

Copy both the key and value at the minimum / maximum position into
`*key_out` and `*val_out` respectively. Either output pointer may be `NULL`
to skip that field. Returns `false` if the map is empty.

Prefer these over `omap_min_key` + `omap_get` when you need both key and
value, since the latter would require two tree walks.

```c
int lo_k, lo_v, hi_k, hi_v;
omap_min_entry(m, &lo_k, &lo_v);
omap_max_entry(m, &hi_k, &hi_v);
printf("range [%d, %d]\n", lo_k, hi_k);
```

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

Ascending key-order [`Iter`](iter.md). Each element is an
[`OMapEntry`](#omapentry) — both `key` and `value` are pointers into live
node storage. Do not modify the map while iterating.

### `omap_iter_rev`

```c
Iter omap_iter_rev(const OMap *m);
```

Descending key-order [`Iter`](iter.md). Same pointer-lifetime rules as
`omap_iter`.

### `omap_iter_range`

```c
Iter omap_iter_range(const OMap *m, const void *lo_key, const void *hi_key);
```

Ascending key-order iteration over entries where `lo_key <= key <= hi_key`.
Pass `NULL` for either bound to leave it open.

```c
int lo = 10, hi = 20;
Iter      it = omap_iter_range(m, &lo, &hi);
OMapEntry e;
while (it.next(&it, &e))
    printf("%d => %.2f\n", *(int *)e.key, *(double *)e.value);
iter_drop(&it);
```

---

### `omap_clear`

```c
void omap_clear(OMap *m);
```

Remove all key-value pairs. Every node is freed back to the allocator (a no-op
for arena allocators) and `root` is set to NULL. The `OMap` struct remains
valid and can be reused immediately.

---

### `omap_free`

```c
void omap_free(OMap *m);
```

Free all nodes and then the OMap struct itself. Do not use `m` after calling this.

---

## Example

```c
Arena *a = arena_create(4096);
OMap  *m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));

for (int i = 1; i <= 5; i++) {
    int v = i * 100;
    omap_set(m, &i, &v);
}

int min_k, max_k;
omap_min_key(m, &min_k);
omap_max_key(m, &max_k);
printf("min=%d max=%d\n", min_k, max_k);  // 1, 5

// iterate all in ascending order
Iter      it = omap_iter(m);
OMapEntry e;
while (it.next(&it, &e))
    printf("%d => %d\n", *(int *)e.key, *(int *)e.value);
iter_drop(&it);

arena_free(a);
```
