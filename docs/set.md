# set

Open-addressing hash set using Robin Hood hashing.

**Header:** `src/set/set.h`  
**See also:** [`hashmap`](hashmap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `set_hash_fn` / `set_eq_fn`

```c
typedef size_t (*set_hash_fn)(const void *key, size_t key_size);
typedef int    (*set_eq_fn)(const void *a, const void *b, size_t key_size);
```

Same signatures as [`hash_fn` / `eq_fn`](hashmap.md#types) in `hashmap`, so
`hashmap_fnv1a`, `hashmap_eq_bytes`, `hashmap_fnv1a_str`, and `hashmap_eq_str`
can be passed directly.

### `Set`

```c
typedef struct {
  SetBucket *buckets;
  size_t     cap;       /* always a power of 2 */
  size_t     len;
  size_t     elem_size;
  set_hash_fn hash;
  set_eq_fn   eq;
  Allocator   allocator;
} Set;
```

---

## Functions

### `set_create`

```c
Set set_create(size_t elem_size, set_hash_fn hash, set_eq_fn eq,
               Allocator allocator);
```

Create an empty set. You must supply a hash function and an equality function.
For integer-sized keys use `hashmap_fnv1a` / `hashmap_eq_bytes` from
[`hashmap.h`](hashmap.md). For `char *` keys use `hashmap_fnv1a_str` /
`hashmap_eq_str`.

```c
#include "hashmap/hashmap.h"

Arena *a = arena_create(4096);
Set    s = set_create(sizeof(int),
                      hashmap_fnv1a, hashmap_eq_bytes,
                      arena_allocator(a));
```

---

### `set_add`

```c
int set_add(Set *s, const void *elem);
```

Add a copy of `elem`. Returns `1` if inserted, `0` if already present.

---

### `set_contains`

```c
int set_contains(const Set *s, const void *elem);
```

Return `1` if `elem` is in the set, `0` otherwise.

---

### `set_remove`

```c
int set_remove(Set *s, const void *elem);
```

Remove `elem`. Returns `1` if removed, `0` if not found.

---

### `set_len`

```c
size_t set_len(const Set *s);
```

---

### `set_iter`

```c
Iter set_iter(const Set *s);
```

Iterate over all elements in unspecified order. Yields elements of size
`elem_size`.

```c
Iter it = set_iter(&s);
int  v;
while (it.next(&it, &v))
    printf("%d\n", v);
iter_drop(&it);
```

---

### `set_free`

```c
void set_free(Set *s);
```

---

## Example

```c
#include "hashmap/hashmap.h"

Arena *a = arena_create(4096);
Set    s = set_create(sizeof(int), hashmap_fnv1a, hashmap_eq_bytes,
                      arena_allocator(a));

int nums[] = {1, 2, 3, 2, 1};
for (int i = 0; i < 5; i++)
    set_add(&s, &nums[i]);

printf("len=%zu\n", set_len(&s));        // 3
printf("%d\n", set_contains(&s, &(int){2})); // 1
printf("%d\n", set_contains(&s, &(int){9})); // 0

arena_free(a);
```
