# ringbuf

Double-ended circular buffer (deque). All push/pop operations are amortised
O(1). Random access via `ringbuf_at` is O(1). The buffer grows automatically
when full.

Because both ends are equally cheap to use, `RingBuf` can serve as a FIFO
queue (push-back / pop-front), a LIFO stack (push-back / pop-back), a deque,
or a sliding-window buffer.

**Header:** `src/ringbuf/ringbuf.h`  
**See also:** [`iter`](iter.md) · [`arena`](arena.md) · [`queue`](queue.md)

---

## Type

### `RingBuf`

```c
typedef struct RingBuf RingBuf;
```

Opaque handle. Backed internally by a power-of-2 flat buffer with a moving
`head` index; no element copies are required for wrap-around.

---

## Functions

### `ringbuf_create`

```c
RingBuf *ringbuf_create(size_t elem_size, Allocator allocator);
```

Create an empty ring buffer. Returns `NULL` if `elem_size` is zero or the
initial allocation fails.

```c
Arena   *a = arena_create(4096);
RingBuf *r = ringbuf_create(sizeof(int), arena_allocator(a));
```

---

### `ringbuf_push_back`

```c
SeqcStatus ringbuf_push_back(RingBuf *r, const void *elem);
```

Append a copy of `elem` to the back (tail). Grows the buffer if full.
Returns `SEQC_OOM` on allocation failure; `SEQC_OK` otherwise.

---

### `ringbuf_push_front`

```c
SeqcStatus ringbuf_push_front(RingBuf *r, const void *elem);
```

Prepend a copy of `elem` to the front (head). Grows the buffer if full.
Returns `SEQC_OOM` on allocation failure; `SEQC_OK` otherwise.

---

### `ringbuf_pop_front`

```c
SeqcStatus ringbuf_pop_front(RingBuf *r, void *out);
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
SeqcStatus ringbuf_pop_back(RingBuf *r, void *out);
```

Remove the back element. Copies it into `*out` if `out` is not `NULL`.
Returns `SEQC_OK` on success, `SEQC_NOT_FOUND` if empty.

---

### `ringbuf_at`

```c
SeqcStatus ringbuf_at(const RingBuf *r, size_t i, void *out);
```

Copy the element at logical index `i` (0 = front) into `*out`. `out` may be
`NULL` to probe bounds only. Returns `SEQC_OK` on success, `SEQC_NOT_FOUND`
if `i >= len`.

```c
int front, back;
ringbuf_at(r, 0, &front);
ringbuf_at(r, ringbuf_len(r) - 1, &back);
```

---

### `ringbuf_len` / `ringbuf_cap` / `ringbuf_is_empty`

```c
size_t ringbuf_len(const RingBuf *r);
size_t ringbuf_cap(const RingBuf *r);
bool   ringbuf_is_empty(const RingBuf *r);
```

`len` — number of elements currently stored.  
`cap` — total slots allocated (always a power of 2).  
`is_empty` — equivalent to `len == 0`.

---

### `ringbuf_iter`

```c
Iter ringbuf_iter(const RingBuf *r);
```

Create a forward [`Iter`](iter.md) from front to back.

```c
Iter it = ringbuf_iter(r);
int  v;
while (it.next(&it, &v))
    printf("%d\n", v);
iter_drop(&it);
```

### `ringbuf_iter_rev`

```c
Iter ringbuf_iter_rev(const RingBuf *r);
```

Create a reverse [`Iter`](iter.md) from back to front.

---

### `ringbuf_clear`

```c
void ringbuf_clear(RingBuf *r);
```

Remove all elements. The allocated buffer is retained, so subsequent pushes
will not reallocate until capacity is exhausted again.

---

### `ringbuf_free`

```c
void ringbuf_free(RingBuf *r);
```

Free the buffer and the `RingBuf` struct itself. Do not use `r` after calling
this.

---

## Example: sliding window

```c
#include "ringbuf/ringbuf.h"
#include "arena/arena.h"

Arena   *a = arena_create(4096);
RingBuf *w = ringbuf_create(sizeof(int), arena_allocator(a));

int stream[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
const size_t WINDOW = 4;

for (size_t i = 0; i < 10; i++) {
    ringbuf_push_back(w, &stream[i]);
    if (ringbuf_len(w) > WINDOW)
        ringbuf_pop_front(w, NULL);

    /* window contains the last min(WINDOW, i+1) elements */
}

arena_free(a);
```

## Example: deque

```c
RingBuf *d = ringbuf_create(sizeof(int), arena_allocator(a));

int vals[] = {3, 4, 5};
for (int i = 0; i < 3; i++) ringbuf_push_back(d, &vals[i]);

int lo = 1, hi = 2;
ringbuf_push_front(d, &lo);   /* [1, 3, 4, 5] */
ringbuf_push_back(d, &hi);    /* [1, 3, 4, 5, 2] */

int out;
ringbuf_pop_front(d, &out);   /* out = 1 */
ringbuf_pop_back(d, &out);    /* out = 2 */
```
