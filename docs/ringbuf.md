# ringbuf

Double-ended circular buffer (deque). All push/pop operations are amortised
O(1). Random access via `ringbuf_get` is O(1). The buffer grows automatically
when full.

Because both ends are equally cheap to use, `ringbuf_t` can serve as a FIFO
queue (push-back / pop-front), a LIFO stack (push-back / pop-back), a deque,
or a sliding-window buffer.

**Header:** `include/seqc/ringbuf.h`  
**See also:** [`iter`](iter.md) · [`arena`](arena.md) · [`queue`](queue.md)

---

## Type

### `ringbuf_t`

```c
typedef struct ringbuf_t ringbuf_t;
```

Opaque handle. Backed internally by a power-of-2 flat buffer with a moving
`head` index; no element copies are required for wrap-around.

---

## Functions

### `ringbuf_create`

```c
ringbuf_t *ringbuf_create(size_t elem_size, allocator_t allocator);
```

Create an empty ring buffer. Returns `NULL` if `elem_size` is zero or the
initial allocation fails.

```c
growing_arena_t arena;
growing_arena_init(&arena, 4096);
allocator_t a = growing_arena_allocator(&arena);
ringbuf_t *r = ringbuf_create(sizeof(int), a);
```

---

### `ringbuf_push_back`

```c
seqc_status_t ringbuf_push_back(ringbuf_t *r, const void *elem);
```

Append a copy of `elem` to the back (tail). Grows the buffer if full.
Returns `SEQC_OOM` on allocation failure; `SEQC_OK` otherwise.

---

### `ringbuf_push_front`

```c
seqc_status_t ringbuf_push_front(ringbuf_t *r, const void *elem);
```

Prepend a copy of `elem` to the front (head). Grows the buffer if full.
Returns `SEQC_OOM` on allocation failure; `SEQC_OK` otherwise.

---

### `ringbuf_pop_front`

```c
seqc_status_t ringbuf_pop_front(ringbuf_t *r, void *out);
```

Remove the front element. Copies it into `*out` if `out` is not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if empty.

```c
int val;
while (ringbuf_pop_front(r, &val) == SEQC_OK)
    printf("%d\n", val);
```

---

### `ringbuf_pop_back`

```c
seqc_status_t ringbuf_pop_back(ringbuf_t *r, void *out);
```

Remove the back element. Copies it into `*out` if `out` is not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if empty.

---

### `ringbuf_get`

```c
seqc_status_t ringbuf_get(const ringbuf_t *r, size_t i, void *out);
```

Copy the element at logical index `i` (0 = front) into `*out`. `out` may be
`NULL` to probe bounds only. Returns `SEQC_OK` on success, `SEQC_NOT_FOUND`
if `i >= len`.

```c
int front, back;
ringbuf_get(r, 0, &front);
ringbuf_get(r, ringbuf_len(r) - 1, &back);
```

---

### `ringbuf_len` / `ringbuf_cap` / `ringbuf_is_empty`

```c
size_t ringbuf_len(const ringbuf_t *r);
size_t ringbuf_cap(const ringbuf_t *r);
bool   ringbuf_is_empty(const ringbuf_t *r);
```

`len` — number of elements currently stored.  
`cap` — total slots allocated (always a power of 2).  
`is_empty` — equivalent to `len == 0`.

---

### `ringbuf_iter`

```c
iter_t ringbuf_iter(const ringbuf_t *r);
```

Create a forward [`iter_t`](iter.md) from front to back.

```c
iter_t it = ringbuf_iter(r);
int  v;
while (it.next(&it, &v))
    printf("%d\n", v);
iter_destroy(&it);
```

### `ringbuf_iter_rev`

```c
iter_t ringbuf_iter_rev(const ringbuf_t *r);
```

Create a reverse [`iter_t`](iter.md) from back to front.

---

### `ringbuf_clear`

```c
void ringbuf_clear(ringbuf_t *r);
```

Remove all elements. The allocated buffer is retained, so subsequent pushes
will not reallocate until capacity is exhausted again.

---

### `ringbuf_destroy`

```c
void ringbuf_destroy(ringbuf_t *r);
```

Free the buffer and the `ringbuf_t` struct itself. Do not use `r` after calling
this.

---

### `ringbuf_get_ptr`, `ringbuf_front`, `ringbuf_back` and their `_ptr` variants

```c
void *ringbuf_get_ptr(const ringbuf_t *r, size_t i);
seqc_status_t ringbuf_front(const ringbuf_t *r, void *out);
seqc_status_t ringbuf_back(const ringbuf_t *r, void *out);
void *ringbuf_front_ptr(const ringbuf_t *r);
void *ringbuf_back_ptr(const ringbuf_t *r);
```

The pointer to element `i` (the pair of `ringbuf_get`), and both ends. See [naming](naming.md#reading-elements-copy-or-pointer): the copy is yours, the pointer is valid only until the next change.

---

### `ringbuf_extend`

```c
seqc_status_t ringbuf_extend(ringbuf_t *r, iter_t it);
```

Push every element of `it` at the back, in order. Add every element of `it`, which is always consumed — also on error and for a `NULL` collection (`SEQC_INVALID`). Stops at the first error and returns it.

---

## Example: sliding window

```c
#include "seqc/ringbuf.h"
#include "seqc/arena.h"

growing_arena_t arena;
growing_arena_init(&arena, 4096);
allocator_t a = growing_arena_allocator(&arena);
ringbuf_t *w = ringbuf_create(sizeof(int), a);

int stream[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
const size_t WINDOW = 4;

for (size_t i = 0; i < 10; i++) {
    ringbuf_push_back(w, &stream[i]);
    if (ringbuf_len(w) > WINDOW)
        ringbuf_pop_front(w, NULL);

    /* window contains the last min(WINDOW, i+1) elements */
}

growing_arena_destroy(&arena);
```

## Example: deque

```c
ringbuf_t *d = ringbuf_create(sizeof(int), a);

int vals[] = {3, 4, 5};
for (int i = 0; i < 3; i++) ringbuf_push_back(d, &vals[i]);

int lo = 1, hi = 2;
ringbuf_push_front(d, &lo);   /* [1, 3, 4, 5] */
ringbuf_push_back(d, &hi);    /* [1, 3, 4, 5, 2] */

int out;
ringbuf_pop_front(d, &out);   /* out = 1 */
ringbuf_pop_back(d, &out);    /* out = 2 */
```
