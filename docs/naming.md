# Naming conventions

seqc 3.0 names every operation the same way in every collection, so knowing
one collection means knowing them all. The same conventions are used across
the library family (arena_allocation, threadc, filec).

## Lifecycle

| Pair | Meaning |
|---|---|
| `X_create(…)` → `X_destroy(x)` | the library allocates the object, through your allocator (`vec_create` / `vec_destroy`) |
| `X_init(&x, …)` → `X_destroy(&x)` | you own the storage, the library sets it up (`growing_arena_init`, `tc_mutex_init`) |
| `X_clear(x)` | empty it; keep the object and its capacity |
| `iter_destroy(&it)` | release an iterator you did not hand to a terminal |

`free` is reserved for raw memory (`mem_free`). `X_destroy(NULL)` is a no-op.

## Size

| Function | Meaning |
|---|---|
| `X_len(x)` | number of elements |
| `X_is_empty(x)` | `true` for an empty collection and for `NULL` |
| `X_cap(x)` | allocated capacity (`vec`, `ringbuf`) |
| `iter_count(it)` | consumes an iterator and counts — not a size query |

## Reading elements: copy or pointer

Every element accessor comes as a pair:

| Form | Returns | Valid |
|---|---|---|
| `X_get(…, void *out)`, `X_peek(…, out)`, `X_front(…, out)`, … | copies the element into `out` and returns a `seqc_status_t` | the copy is yours — **not affected by later changes** |
| `X_get_ptr(…)`, `X_peek_ptr(…)`, `X_front_ptr(…)`, … | a pointer into the collection, `NULL` if absent | **only until the next change** to the collection |

For every copy accessor: `SEQC_OK` when found, `SEQC_NOT_FOUND` when absent
(out of range, empty, missing key), `SEQC_INVALID` for a `NULL` collection
(or key); `out` may be `NULL` to test for presence only, and is left
untouched when nothing is found.

**Use the copy by default.** What counts as a "change" depends on the
collection, and in some it is not visible from the code:

| Collection | What moves an element |
|---|---|
| `vec`, `stack`, `pqueue`, `queue`, `ringbuf` | adding beyond capacity (reallocation), inserting/removing in the middle, `pqueue` reordering on every push/pop |
| `hashmap`, `set` | **any** add or remove — robin-hood hashing moves *other* entries too |
| `avl`, `bstree`, `omap` | removing an element (nodes are reused) |
| `list`, `dlist` | only removing that element — nodes never move |

**Use the pointer** to change a value in place, or to read large elements in
a tight loop without copying them — and let go of it before the next change:

```c
(*(int *)hashmap_get_ptr(counts, &word))++;
```

A copy is shallow: an element holding a pointer (a `string_t`, say) copies
the pointer, not what it points at.

| Position | Copy | Pointer |
|---|---|---|
| index | `vec_get`, `slice_get`, `ringbuf_get` | `…_get_ptr` |
| key | `hashmap_get`, `omap_get` | `…_get_ptr` |
| the end `pop` takes | `stack_peek`, `pqueue_peek` | `…_peek_ptr` |
| either end | `X_front`, `X_back` (`queue`, `list`, `dlist`, `ringbuf`) | `…_front_ptr`, `…_back_ptr` |
| order | `avl_min/max`, `bstree_min/max`, `omap_min_key/max_key` | `…_ptr` |

`X_pop…` always copies (the element is gone afterwards).

The same split holds for whole strings: a function named `to_…` with an
allocator returns a copy that is yours (`string_to_cstr`, `strbuf_to_string`),
a `view` borrows (`string_view_cstr`, `strbuf_view`). Every returned copy is a
plain allocation of exactly its length, freed with `mem_free` — see
[Who owns a result](string.md#who-owns-a-result).

## Changing

| Verb | Used for |
|---|---|
| `push` / `pop` (`_front`, `_back`) | the ends of sequences |
| `insert` / `remove` | a position (`vec_insert(v, i, …)`) |
| `add` / `remove` | sets: `set`, `avl`, `bstree` (`SEQC_DUPLICATE` if present) |
| `set` / `remove` | maps: `hashmap`, `omap` (insert or update) |
| `X_extend(x, it)` | add every element of an iterator, the collection's own way (push, push_back, add, set); the iterator is always consumed |

## Searching

| Function | Meaning |
|---|---|
| `X_contains(x, value)` | is this value / key present? (sets, maps, trees) |
| `X_find(x, pred, ctx)` | pointer to the first element matching a predicate (`vec`, `slice`, `iter`) |
| `X_any(x, pred, ctx)` | does any element match a predicate? (`vec`, `slice`, `iter`) |

## Iterating

`X_iter`, `X_iter_rev` (where the order can be reversed), `X_iter_range`
(ordered collections). Adaptors and terminals live in [iter](iter.md).

## Types

Types are `snake_case_t` without a prefix — except `seqc_stack_t`: POSIX
reserves `stack_t` in `<signal.h>`.
