# hashmap

Open-addressing hash map using Robin Hood hashing.

**Header:** `src/hashmap/hashmap.h`  
**See also:** [`set`](set.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types {#types}

### `hash_fn` / `eq_fn`

```c
typedef size_t (*hash_fn)(const void *key, size_t key_size);
typedef bool   (*eq_fn)(const void *a, const void *b, size_t key_size);
```

### `HashMap`

```c
typedef struct HashMap HashMap;
```

Opaque handle.

### `HashMapEntry`

```c
typedef struct {
  void *key;
  void *value;
} HashMapEntry;
```

Yielded by [`hashmap_iter`](#hashmap_iter). Both pointers point
directly into live bucket storage — do not modify the map while iterating.

---

## Built-in hash / equality helpers

### `hashmap_fnv1a`

```c
size_t hashmap_fnv1a(const void *key, size_t key_size);
```

FNV-1a hash over the raw bytes of the key. Suitable for any fixed-size type
(`int`, `size_t`, structs, …).

### `hashmap_eq_bytes`

```c
bool hashmap_eq_bytes(const void *a, const void *b, size_t key_size);
```

Bytewise equality. Suitable for any fixed-size type.

### `hashmap_fnv1a_str`

```c
size_t hashmap_fnv1a_str(const void *key, size_t key_size);
```

Hash a `char *` key by hashing the pointed-to string rather than the pointer
value. Use when the key type is `char *` (i.e. `key_size = sizeof(char *)`).

### `hashmap_eq_str`

```c
bool hashmap_eq_str(const void *a, const void *b, size_t key_size);
```

`strcmp`-based equality for `char *` keys.

> For [`String`](string.md) keys use `string_hash` and `string_key_eq` from
> `string/string.h`.

---

## Functions

### `hashmap_create`

```c
HashMap *hashmap_create(size_t key_size, size_t val_size,
                        hash_fn hash, eq_fn eq,
                        Allocator allocator);
```

Create an empty hash map. Returns `NULL` if `key_size` or `val_size` is zero.

```c
Arena   *a = arena_create(4096);
HashMap *m = hashmap_create(sizeof(int), sizeof(double),
                             hashmap_fnv1a, hashmap_eq_bytes,
                             arena_allocator(a));
```

---

### `hashmap_set`

```c
bool hashmap_set(HashMap *map, const void *key, const void *value);
```

Insert or update. Returns `true` if a new key was inserted, `false` if an existing
key was updated.

```c
int    k = 42;
double v = 3.14;
hashmap_set(m, &k, &v);
```

---

### `hashmap_get`

```c
void *hashmap_get(const HashMap *map, const void *key);
```

Return a pointer to the stored value, or `NULL` if not found.

```c
double *val = hashmap_get(m, &k);
if (val) printf("%.2f\n", *val);
```

---

### `hashmap_delete`

```c
bool hashmap_delete(HashMap *map, const void *key);
```

Remove a key. Returns `true` if removed, `false` if not found.

---

### `hashmap_contains`

```c
bool hashmap_contains(const HashMap *map, const void *key);
```

Return `true` if `key` is present in the map. Equivalent to
`hashmap_get(map, key) != NULL` but expresses intent more clearly.

```c
if (hashmap_contains(m, &k))
    printf("key is present\n");
```

---

### `hashmap_len`

```c
size_t hashmap_len(const HashMap *map);
```

---

### `hashmap_iter` {#hashmap_iter}

```c
Iter hashmap_iter(const HashMap *map);
```

Iterate over all key-value pairs in unspecified order. Each element is a
[`HashMapEntry`](#hashmapmapentry) — both `key` and `value` are pointers into
live bucket storage.

```c
Iter        it = hashmap_iter(m);
HashMapEntry e;
while (it.next(&it, &e)) {
    printf("%d => %.2f\n", *(int *)e.key, *(double *)e.value);
}
iter_drop(&it);
```

---

### `hashmap_iter_rev`

```c
Iter hashmap_iter_rev(const HashMap *map);
```

Iterate over all key-value pairs in reverse bucket-storage order.

---

### `hashmap_clear`

```c
void hashmap_clear(HashMap *map);
```

Remove all entries. Key and value copies are freed (no-op for arena
allocators) and the bucket array is zeroed. The allocated bucket buffer is
retained, so subsequent insertions will not reallocate until the load factor
threshold is reached again.

---

### `hashmap_free`

```c
void hashmap_free(HashMap *map);
```

Free all key/value copies, the bucket array, and the HashMap struct itself.
Do not use `map` after calling this.

---

## Example: string keys

```c
#include "hashmap/hashmap.h"

Arena   *a = arena_create(4096);
HashMap *m = hashmap_create(sizeof(char *), sizeof(int),
                             hashmap_fnv1a_str, hashmap_eq_str,
                             arena_allocator(a));

const char *keys[]   = {"apple", "banana", "cherry"};
int         counts[] = {3, 1, 7};

for (int i = 0; i < 3; i++)
    hashmap_set(m, &keys[i], &counts[i]);

const char *k = "banana";
int *count = hashmap_get(m, &k);
printf("%d\n", *count);  // 1

arena_free(a);
```
