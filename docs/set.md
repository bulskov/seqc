# set

Open-addressing hash set using Robin Hood hashing.

**Header:** `src/set/set.h`  
**See also:** [`hashmap`](hashmap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `Set`

```c
typedef struct Set Set;
```

Opaque handle.

---

## Functions

### `set_create`

```c
Set *set_create(size_t elem_size, hash_fn hash, eq_fn eq,
                Allocator allocator);
```

Create an empty set. Returns `NULL` if `elem_size` is zero. You must supply a
hash function and an equality function. `hash_fn` and `eq_fn` are defined in
[`iter/iter.h`](iter.md#function-pointer-types) (included transitively).
For integer-sized keys use `hashmap_fnv1a` / `hashmap_eq_bytes` from
[`hashmap.h`](hashmap.md). For `char *` keys use `hashmap_fnv1a_str` /
`hashmap_eq_str`.

```c
#include "hashmap/hashmap.h"

Arena *a = arena_create(4096);
Set   *s = set_create(sizeof(int),
                      hashmap_fnv1a, hashmap_eq_bytes,
                      arena_allocator(a));
```

---

### `set_add`

```c
bool set_add(Set *s, const void *elem);
```

Add a copy of `elem`. Returns `true` if inserted, `false` if already present.

---

### `set_contains`

```c
bool set_contains(const Set *s, const void *elem);
```

Return `true` if `elem` is in the set, `false` otherwise.

---

### `set_remove`

```c
bool set_remove(Set *s, const void *elem);
```

Remove `elem`. Returns `true` if removed, `false` if not found.

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
Iter it = set_iter(s);
int  v;
while (it.next(&it, &v))
    printf("%d\n", v);
iter_drop(&it);
```

### `set_iter_rev`

```c
Iter set_iter_rev(const Set *s);
```

Iterate over all elements in reverse bucket-storage order. Same element set as
`set_iter`, different traversal direction. Useful when you want to consume in
the opposite order without collecting first.

```c
Iter it = set_iter_rev(s);
int  v;
while (it.next(&it, &v))
    printf("%d\n", v);
iter_drop(&it);
```

---

### `set_clear`

```c
void set_clear(Set *s);
```

Remove all elements. Key copies are freed (no-op for arena allocators) and the
bucket array is zeroed. The allocated bucket buffer is retained, so subsequent
insertions will not reallocate until the load factor threshold is reached
again.

---

### `set_free`

```c
void set_free(Set *s);
```

Free all key copies, the bucket array, and the Set struct itself.
Do not use `s` after calling this.

---

## Example

```c
#include "hashmap/hashmap.h"

Arena *a = arena_create(4096);
Set   *s = set_create(sizeof(int), hashmap_fnv1a, hashmap_eq_bytes,
                      arena_allocator(a));

int nums[] = {1, 2, 3, 2, 1};
for (int i = 0; i < 5; i++)
    set_add(s, &nums[i]);

printf("len=%zu\n", set_len(s));        // 3
printf("%d\n", set_contains(s, &(int){2})); // 1
printf("%d\n", set_contains(s, &(int){9})); // 0

arena_free(a);
```
