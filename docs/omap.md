# omap

Ordered map backed by an [AVL tree](avl.md). Keys are kept sorted by the
supplied comparator. All operations are O(log n).

**Header:** `include/seqc/omap.h`  
**See also:** [`avl`](avl.md) · [`hashmap`](hashmap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `omap_entry_t`

```c
typedef map_entry_t omap_entry_t;
```

Alias for [`map_entry_t`](iter.md#mapentry) (defined in `iter/iter.h`).
Yielded by the iterators. Both pointers point directly into live node storage —
do not modify the map while iterating.

### `omap_t`

```c
typedef struct omap_t omap_t;
```

Opaque handle.

---

## Functions

### `omap_create`

```c
omap_t *omap_create(size_t key_size, size_t val_size,
                  compare_fn cmp, allocator_t allocator);
```

Create an empty ordered map. Returns `NULL` if allocation fails.

```c
static int int_cmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

Arena *a = arena_create(4096);
omap_t  *m = omap_create(sizeof(int), sizeof(double),
                        int_cmp, arena_allocator(a));
```

---

### `omap_set`

```c
seqc_status_t omap_set(omap_t *m, const void *key, const void *value);
```

Insert or update. Returns `SEQC_OK` whether inserting a new key or updating
an existing one. Returns `SEQC_OOM` if an allocation fails. Returns
`SEQC_INVALID` if `m`, `key`, or `value` is `NULL`.

```c
int    k = 1;
double v = 3.14;
omap_set(m, &k, &v);
```

---

### `omap_get`

```c
seqc_status_t omap_get(const omap_t *m, const void *key, void *out);
```

Copy the value for `key` into `*out`. `out` may be `NULL` to test for
presence only. Returns `SEQC_OK` if found, `SEQC_NOT_FOUND` otherwise.
Returns `SEQC_INVALID` if `m` or `key` is `NULL`.

```c
double val;
if (omap_get(m, &(int){1}, &val) == SEQC_OK)
    printf("%.2f\n", val);
```

---

### `omap_contains`

```c
bool omap_contains(const omap_t *m, const void *key);
```

---

### `omap_remove`

```c
seqc_status_t omap_remove(omap_t *m, const void *key);
```

Remove by key. Returns `SEQC_OK` if removed, `SEQC_NOT_FOUND` if absent,
`SEQC_INVALID` if `m` or `key` is `NULL`.

---

### `omap_min_key` / `omap_max_key`

```c
seqc_status_t omap_min_key(const omap_t *m, void *out);
seqc_status_t omap_max_key(const omap_t *m, void *out);
```

Copy the smallest / largest key into `*out`. Returns `SEQC_NOT_FOUND` if the
map is empty; `out` may be `NULL` to test for non-emptiness. Returns
`SEQC_OK` on success.

---

### `omap_min_entry` / `omap_max_entry`

```c
seqc_status_t omap_min_entry(const omap_t *m, void *key_out, void *val_out);
seqc_status_t omap_max_entry(const omap_t *m, void *key_out, void *val_out);
```

Copy both the key and value at the minimum / maximum position into
`*key_out` and `*val_out` respectively. Either output pointer may be `NULL`
to skip that field. Returns `SEQC_NOT_FOUND` if the map is empty;
`SEQC_OK` on success.

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
size_t omap_len(const omap_t *m);
int    omap_height(const omap_t *m);   /* 0 if empty */
```

---

### `omap_iter`

```c
iter_t omap_iter(const omap_t *m);
```

Ascending key-order [`iter_t`](iter.md). Each element is an
[`omap_entry_t`](#omapentry) — both `key` and `value` are pointers into live
node storage. Do not modify the map while iterating.

### `omap_iter_rev`

```c
iter_t omap_iter_rev(const omap_t *m);
```

Descending key-order [`iter_t`](iter.md). Same pointer-lifetime rules as
`omap_iter`.

### `omap_iter_range`

```c
iter_t omap_iter_range(const omap_t *m, const void *lo_key, const void *hi_key);
```

Ascending key-order iteration over entries where `lo_key <= key <= hi_key`.
Pass `NULL` for either bound to leave it open.

```c
int lo = 10, hi = 20;
iter_t      it = omap_iter_range(m, &lo, &hi);
omap_entry_t e;
while (it.next(&it, &e))
    printf("%d => %.2f\n", *(int *)e.key, *(double *)e.value);
iter_drop(&it);
```

---

### `omap_clear`

```c
void omap_clear(omap_t *m);
```

Remove all key-value pairs. Every node is freed back to the allocator (a no-op
for arena allocators) and `root` is set to NULL. The `omap_t` struct remains
valid and can be reused immediately.

---

### `omap_free`

```c
void omap_free(omap_t *m);
```

Free all nodes and then the omap_t struct itself. Do not use `m` after calling this.

---

## Example

```c
Arena *a = arena_create(4096);
omap_t  *m = omap_create(sizeof(int), sizeof(int), int_cmp, arena_allocator(a));

for (int i = 1; i <= 5; i++) {
    int v = i * 100;
    omap_set(m, &i, &v);
}

int min_k, max_k;
omap_min_key(m, &min_k);
omap_max_key(m, &max_k);
printf("min=%d max=%d\n", min_k, max_k);  // 1, 5

// iterate all in ascending order
iter_t      it = omap_iter(m);
omap_entry_t e;
while (it.next(&it, &e))
    printf("%d => %d\n", *(int *)e.key, *(int *)e.value);
iter_drop(&it);

arena_free(a);
```
