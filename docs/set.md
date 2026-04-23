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
SeqcStatus set_add(Set *s, const void *elem);
```

Add a copy of `elem`. Returns `SEQC_OK` if inserted, `SEQC_DUPLICATE` if
already present (no-op), `SEQC_OOM` on allocation failure.

---

### `set_contains`

```c
bool set_contains(const Set *s, const void *elem);
```

Return `true` if `elem` is in the set, `false` otherwise.

---

### `set_remove`

```c
SeqcStatus set_remove(Set *s, const void *elem);
```

Remove `elem`. Returns `SEQC_OK` if removed, `SEQC_NOT_FOUND` if absent.

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

### `set_add_all`

```c
SeqcStatus set_add_all(Set *s, Iter it);
```

Drain `it`, adding each element into `s`. Duplicates are silently skipped.
Returns `SEQC_OOM` if an allocation fails; the iterator is always dropped.

```c
/* Copy all elements from another set */
set_add_all(dst, set_iter(src));
```

---

## Set algebra

All three operations write results into a pre-created (usually empty) `dest`
set. The `dest`, `a`, and `b` sets must use compatible hash and equality
functions. Return `SEQC_OOM` on the first allocation failure.

### `set_union`

```c
SeqcStatus set_union(Set *dest, const Set *a, const Set *b);
```

Set `dest = a ∪ b`. All elements present in either `a` or `b` are added to
`dest`. Duplicates are silently skipped.

```c
Set *u = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
set_union(u, s1, s2);
```

### `set_intersection`

```c
SeqcStatus set_intersection(Set *dest, const Set *a, const Set *b);
```

Set `dest = a ∩ b`. Only elements present in **both** `a` and `b` are added.
Iterates the smaller set for efficiency.

### `set_difference`

```c
SeqcStatus set_difference(Set *dest, const Set *a, const Set *b);
```

Set `dest = a \ b`. Elements present in `a` but **not** in `b` are added.

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

## Health and diagnostics

Same design as [`hashmap`](hashmap.md#health-and-diagnostics): `max_psl` is
tracked at zero cost and used as a secondary resize trigger.

### `SET_PSL_THRESHOLD`

```c
#define SET_PSL_THRESHOLD 128
```

### `SetStats`

```c
typedef struct {
    size_t  len;
    size_t  cap;
    double  load_factor;
    uint8_t max_psl;
    double  mean_psl;
    bool    is_healthy; /* mean_psl < 3.0 and max_psl <= SET_PSL_THRESHOLD/2 */
} SetStats;
```

### `set_is_healthy`

```c
bool set_is_healthy(const Set *s);
```

O(1). Returns `false` when `max_psl > SET_PSL_THRESHOLD / 2`.

### `set_audit`

```c
SetStats set_audit(const Set *s);
```

O(n) full scan. Returns a `SetStats` with `len`, `cap`, `load_factor`,
`max_psl`, `mean_psl`, and `is_healthy`.

```c
SetStats st = set_audit(s);
printf("load=%.2f  max_psl=%u  mean_psl=%.2f  healthy=%s\n",
       st.load_factor, st.max_psl, st.mean_psl,
       st.is_healthy ? "yes" : "no");
```

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
