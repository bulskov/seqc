# set

Open-addressing hash set using Robin Hood hashing.

**Header:** `include/seqc/set.h`  
**See also:** [`hashmap`](hashmap.md) · [`iter`](iter.md) · [`arena`](arena.md)

---

## Types

### `set_t`

```c
typedef struct set_t set_t;
```

Opaque handle.

---

## Functions

### `set_create`

```c
set_t *set_create(size_t elem_size, hash_fn hash, eq_fn eq,
                allocator_t allocator);
```

Create an empty set. Returns `NULL` if `elem_size` is zero. You must supply a
hash function and an equality function. `hash_fn` and `eq_fn` are defined in
[`iter/iter.h`](iter.md#function-pointer-types) (included transitively).
For integer-sized keys use `hash_fnv1a` / `hash_eq_bytes` from
[`iter/hash.h`](hashmap.md#built-in-hash--equality-helpers). For `char *` keys
use `hash_fnv1a_str` / `hash_eq_str`.

```c
#include "seqc/hash.h"

Arena *a = arena_create(4096);
set_t   *s = set_create(sizeof(int),
                      hash_fnv1a, hash_eq_bytes,
                      arena_allocator(a));
```

---

### `set_add`

```c
seqc_status_t set_add(set_t *s, const void *elem);
```

Add a copy of `elem`. Returns `SEQC_OK` if inserted, `SEQC_DUPLICATE` if
already present (no-op), `SEQC_OOM` on allocation failure.

---

### `set_contains`

```c
bool set_contains(const set_t *s, const void *elem);
```

Return `true` if `elem` is in the set, `false` otherwise.

---

### `set_remove`

```c
seqc_status_t set_remove(set_t *s, const void *elem);
```

Remove `elem`. Returns `SEQC_OK` if removed, `SEQC_NOT_FOUND` if absent.

---

### `set_len` / `set_is_empty`

```c
size_t set_len(const set_t *s);
bool   set_is_empty(const set_t *s);
```

---

### `set_iter`

```c
iter_t set_iter(const set_t *s);
```

Iterate over all elements in unspecified order. Yields elements of size
`elem_size`.

```c
iter_t it = set_iter(s);
int  v;
while (it.next(&it, &v))
    printf("%d\n", v);
iter_drop(&it);
```

### `set_iter_rev`

```c
iter_t set_iter_rev(const set_t *s);
```

Iterate over all elements in reverse bucket-storage order. Same element set as
`set_iter`, different traversal direction. Useful when you want to consume in
the opposite order without collecting first.

```c
iter_t it = set_iter_rev(s);
int  v;
while (it.next(&it, &v))
    printf("%d\n", v);
iter_drop(&it);
```

---

### `set_add_all`

```c
seqc_status_t set_add_all(set_t *s, iter_t it);
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
seqc_status_t set_union(set_t *dest, const set_t *a, const set_t *b);
```

Set `dest = a ∪ b`. All elements present in either `a` or `b` are added to
`dest`. Duplicates are silently skipped.

```c
set_t *u = set_create(sizeof(int), int_hash, int_eq, arena_allocator(a));
set_union(u, s1, s2);
```

### `set_intersection`

```c
seqc_status_t set_intersection(set_t *dest, const set_t *a, const set_t *b);
```

Set `dest = a ∩ b`. Only elements present in **both** `a` and `b` are added.
Iterates the smaller set for efficiency.

### `set_difference`

```c
seqc_status_t set_difference(set_t *dest, const set_t *a, const set_t *b);
```

Set `dest = a \ b`. Elements present in `a` but **not** in `b` are added.

```c
void set_clear(set_t *s);
```

Remove all elements. Key copies are freed (no-op for arena allocators) and the
bucket array is zeroed. The allocated bucket buffer is retained, so subsequent
insertions will not reallocate until the load factor threshold is reached
again.

---

### `set_free`

```c
void set_free(set_t *s);
```

Free all key copies, the bucket array, and the set struct itself.
Do not use `s` after calling this.

---

## Health and diagnostics

Same design as [`hashmap`](hashmap.md#health-and-diagnostics): `max_psl` is
tracked at zero cost and used as a secondary resize trigger.

### `SET_PSL_THRESHOLD`

```c
#define SET_PSL_THRESHOLD 128
```

### `set_stats_t`

```c
typedef struct {
    size_t  len;
    size_t  cap;
    double  load_factor;
    uint8_t max_psl;
    double  mean_psl;
    bool    is_healthy; /* mean_psl < 3.0 and max_psl <= SET_PSL_THRESHOLD/2 */
} set_stats_t;
```

### `set_is_healthy`

```c
bool set_is_healthy(const set_t *s);
```

O(1). Returns `false` when `max_psl > SET_PSL_THRESHOLD / 2`.

### `set_audit`

```c
set_stats_t set_audit(const set_t *s);
```

O(n) full scan. Returns a `set_stats_t` with `len`, `cap`, `load_factor`,
`max_psl`, `mean_psl`, and `is_healthy`.

```c
set_stats_t st = set_audit(s);
printf("load=%.2f  max_psl=%u  mean_psl=%.2f  healthy=%s\n",
       st.load_factor, st.max_psl, st.mean_psl,
       st.is_healthy ? "yes" : "no");
```

---

## Example

```c
#include "seqc/hash.h"

Arena *a = arena_create(4096);
set_t   *s = set_create(sizeof(int), hash_fnv1a, hash_eq_bytes,
                      arena_allocator(a));

int nums[] = {1, 2, 3, 2, 1};
for (int i = 0; i < 5; i++)
    set_add(s, &nums[i]);

printf("len=%zu\n", set_len(s));        // 3
printf("%d\n", set_contains(s, &(int){2})); // 1
printf("%d\n", set_contains(s, &(int){9})); // 0

arena_free(a);
```
