# hashmap

Open-addressing hash map using Robin Hood hashing.

**Header:** `include/seqc/hashmap.h`  
**See also:** [`set`](set.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types {#types}

### `hash_fn` / `eq_fn`

```c
typedef size_t (*hash_fn)(const void *key, size_t key_size);
typedef bool   (*eq_fn)(const void *a, const void *b, size_t key_size);
```

Defined in `iter/iter.h` (included transitively). Shared with [`set`](set.md).

### `hashmap_t`

```c
typedef struct hashmap_t hashmap_t;
```

Opaque handle.

### `hashmap_entry_t`

```c
typedef map_entry_t hashmap_entry_t;
```

Alias for [`map_entry_t`](iter.md#mapentry) (defined in `iter/iter.h`).
Yielded by [`hashmap_iter`](#hashmap_iter). Both pointers point
directly into live bucket storage — do not modify the map while iterating.

---

## Built-in hash / equality helpers

These live in `iter/hash.h` (included via `hashmap/hashmap.h`).

### `hash_fnv1a`

```c
size_t hash_fnv1a(const void *key, size_t key_size);
```

FNV-1a hash over the raw bytes of the key. Suitable for any fixed-size type
(`int`, `size_t`, structs, …).

### `hash_eq_bytes`

```c
bool hash_eq_bytes(const void *a, const void *b, size_t key_size);
```

Bytewise equality. Suitable for any fixed-size type.

### `hash_fnv1a_str`

```c
size_t hash_fnv1a_str(const void *key, size_t key_size);
```

Hash a `char *` key by hashing the pointed-to string rather than the pointer
value. Use when the key type is `char *` (i.e. `key_size = sizeof(char *)`).

### `hash_eq_str`

```c
bool hash_eq_str(const void *a, const void *b, size_t key_size);
```

`strcmp`-based equality for `char *` keys.

> For [`string_t`](string.md) keys use `string_hash` and `string_key_eq` from
> `string/string.h`.

---

## Functions

### `hashmap_create`

```c
hashmap_t *hashmap_create(size_t key_size, size_t val_size,
                        hash_fn hash, eq_fn eq,
                        allocator_t allocator);
```

Create an empty hash map. Returns `NULL` if `key_size` or `val_size` is zero, if `hash` or `eq` is `NULL`, if `allocator.alloc` is `NULL`, or if an allocation fails.

```c
Arena   *a = arena_create(4096);
hashmap_t *m = hashmap_create(sizeof(int), sizeof(double),
                             hash_fnv1a, hash_eq_bytes,
                             arena_allocator(a));
```

---

### `hashmap_set`

```c
seqc_status_t hashmap_set(hashmap_t *map, const void *key, const void *value);
```

Insert or update. Returns `SEQC_OK` whether inserting a new key or updating
an existing one. Returns `SEQC_OOM` if an allocation fails. Returns
`SEQC_INVALID` if `map`, `key`, or `value` is `NULL`.

```c
int    k = 42;
double v = 3.14;
hashmap_set(m, &k, &v);
```

---

### `hashmap_get`

```c
seqc_status_t hashmap_get(const hashmap_t *map, const void *key, void *out);
```

Copy the value for `key` into `*out`. `out` may be `NULL` to test for
presence only. Returns `SEQC_OK` if found, `SEQC_NOT_FOUND` otherwise.
Returns `SEQC_INVALID` if `map` or `key` is `NULL`.

```c
double val;
if (hashmap_get(m, &k, &val) == SEQC_OK)
    printf("%.2f\n", val);
```

---

### `hashmap_delete`

```c
seqc_status_t hashmap_delete(hashmap_t *map, const void *key);
```

Remove a key. Returns `SEQC_OK` if removed, `SEQC_NOT_FOUND` if absent,
`SEQC_INVALID` if `map` or `key` is `NULL`.

---

### `hashmap_contains`

```c
bool hashmap_contains(const hashmap_t *map, const void *key);
```

Return `true` if `key` is present in the map. Equivalent to
`hashmap_get(map, key, NULL)`.

```c
if (hashmap_contains(m, &k))
    printf("key is present\n");
```

---

### `hashmap_len`

```c
size_t hashmap_len(const hashmap_t *map);
```

---

### `hashmap_is_empty`

```c
bool hashmap_is_empty(const hashmap_t *map);
```

Returns `true` if the map contains no entries. Equivalent to
`hashmap_len(map) == 0`.

---

### `hashmap_iter` {#hashmap_iter}

```c
iter_t hashmap_iter(const hashmap_t *map);
```

Iterate over all key-value pairs in unspecified order. Each element is a
[`hashmap_entry_t`](#hashmapentry) — both `key` and `value` are pointers into
live bucket storage. Do not modify the map while iterating.

```c
iter_t         it = hashmap_iter(m);
hashmap_entry_t e;
while (it.next(&it, &e)) {
    printf("%d => %.2f\n", *(int *)e.key, *(double *)e.value);
}
iter_drop(&it);
```

---

### `hashmap_iter_rev`

```c
iter_t hashmap_iter_rev(const hashmap_t *map);
```

Iterate over all key-value pairs in reverse bucket-storage order.

---

### `hashmap_set_all`

```c
seqc_status_t hashmap_set_all(hashmap_t *map, iter_t it);
```

Drain `it` (which must yield `hashmap_entry_t` values), inserting each key-value
pair into `map`. Returns `SEQC_OOM` on the first allocation failure.
Returns `SEQC_INVALID` if `map` is `NULL`. The iterator is always dropped.

```c
/* Merge map b into map a */
hashmap_set_all(a, hashmap_iter(b));
```

---

### `hashmap_clear`

```c
void hashmap_clear(hashmap_t *map);
```

Remove all entries. Key and value copies are freed (no-op for arena
allocators) and the bucket array is zeroed. The allocated bucket buffer is
retained, so subsequent insertions will not reallocate until the load factor
threshold is reached again.

---

### `hashmap_free`

```c
void hashmap_free(hashmap_t *map);
```

Free all key/value copies, the bucket array, and the hashmap_t struct itself.
Do not use `map` after calling this.

---

## Health and diagnostics

Robin Hood hashing keeps probe-sequence lengths (PSLs) short for well-distributed
hash functions. A high PSL indicates a poor or degenerate hash. The map tracks
`max_psl` internally at zero cost and uses it as a secondary resize trigger.

### `HASH_PSL_THRESHOLD`

```c
#define HASH_PSL_THRESHOLD 128
```

Defined in `iter/hash.h`. When `max_psl` reaches this value during an insert
the table is resized and rehashed immediately — well before the `uint8_t` PSL
field could wrap to 0 and silently corrupt the table.

### `hashmap_stats_t`

```c
typedef struct {
    size_t  len;
    size_t  cap;
    double  load_factor;
    uint8_t max_psl;    /* worst-case probe length over all stored buckets */
    double  mean_psl;   /* average probe length over occupied buckets */
    bool    is_healthy; /* mean_psl < 3.0 and max_psl <= HASH_PSL_THRESHOLD/2 */
} hashmap_stats_t;
```

### `hashmap_is_healthy`

```c
bool hashmap_is_healthy(const hashmap_t *map);
```

O(1) check. Returns `false` when `max_psl > HASH_PSL_THRESHOLD / 2` (64),
which is a strong signal of a poor hash function. Call after a bulk-load
phase to validate your hash.

```c
if (!hashmap_is_healthy(m))
    fprintf(stderr, "warning: high PSL detected — check your hash function\n");
```

### `hashmap_audit`

```c
hashmap_stats_t hashmap_audit(const hashmap_t *map);
```

O(n) full scan. Returns a [`hashmap_stats_t`](#hashmapstats) struct with a
complete picture of the table's internal state. `mean_psl` is the most
useful signal: with a good hash at 75% load it should be close to 1.5;
a degenerate all-same hash pushes it toward `len / 2`.

```c
hashmap_stats_t s = hashmap_audit(m);
printf("load=%.2f  max_psl=%u  mean_psl=%.2f  healthy=%s\n",
       s.load_factor, s.max_psl, s.mean_psl,
       s.is_healthy ? "yes" : "no");
```

---

## Example: string keys

```c
#include "seqc/hashmap.h"

Arena   *a = arena_create(4096);
hashmap_t *m = hashmap_create(sizeof(char *), sizeof(int),
                             hash_fnv1a_str, hash_eq_str,
                             arena_allocator(a));

const char *keys[]   = {"apple", "banana", "cherry"};
int         counts[] = {3, 1, 7};

for (int i = 0; i < 3; i++)
    hashmap_set(m, &keys[i], &counts[i]);

const char *k = "banana";
int count;
if (hashmap_get(m, &k, &count) == SEQC_OK)
    printf("%d\n", count);  // 1

arena_free(a);
```
