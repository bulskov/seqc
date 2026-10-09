/* The seqc 3.0 conventions (docs/naming.md), checked across every
 * collection:
 *
 *   accessor pairs  X        copies into out, returns a status — not
 *                            affected by later changes; out may be NULL
 *                            (presence test); NULL collection: INVALID
 *                   X_ptr    pointer into the collection, NULL if absent —
 *                            valid until the next change
 *   X_is_empty      true for NULL and for an empty collection
 *   X_extend        drains an iterator (always consumed, even on error)
 *
 * Most tests store ints from an arena.  The extend-on-NULL tests take
 * their iterator from the malloc-based sys_allocator, so LeakSanitizer
 * (./test.sh asan) proves the iterator is destroyed on that path too. */

#include "ctt.h"

#include <string.h>

#include "arena/growing_arena.h"
#include "seqc/avl.h"
#include "seqc/bstree.h"
#include "seqc/dlist.h"
#include "seqc/hash.h"
#include "seqc/hashmap.h"
#include "seqc/list.h"
#include "seqc/omap.h"
#include "seqc/pqueue.h"
#include "seqc/queue.h"
#include "seqc/ringbuf.h"
#include "seqc/slice.h"
#include "seqc/stack.h"
#include "seqc/string.h"
#include "seqc/vec.h"

#include "../oom_alloc.h"

static growing_arena_t arena;
static allocator_t A;

void ctt_before_each(void)
{
    growing_arena_init(&arena, 4096);
    A = growing_arena_allocator(&arena);
}

void ctt_after_each(void)
{
    growing_arena_destroy(&arena);
}

static int int_cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* An iterator over the ints in values, from a vec in the given allocator. */
static iter_t ints(allocator_t alloc, const int *values, size_t n)
{
    vec_t *v = vec_create(sizeof(int), alloc);
    for (size_t i = 0; i < n; ++i)
    {
        vec_push(v, &values[i]);
    }
    return vec_iter(v);
}

static const int ONE_TO_FIVE[] = {1, 2, 3, 4, 5};

/* --- accessor pairs ------------------------------------------------------ */

/* The contract for one pair, given the collection holds `expected` there:
 * the copy returns OK and writes it, accepts out == NULL, and the pointer
 * points at the same value. */
#define CHECK_PAIR(copy_call, ptr_call, expected)                              \
    do                                                                         \
    {                                                                          \
        int got = -1;                                                          \
        ASSERT_EQ(SEQC_OK, copy_call(&got));                                   \
        ASSERT_EQ((expected), got);                                            \
        ASSERT_EQ(SEQC_OK, copy_call(NULL));                                   \
        int *p = (int *)(ptr_call);                                            \
        ASSERT_NOT_NULL(p);                                                    \
        ASSERT_EQ((expected), *p);                                             \
    } while (0)

/* Empty: the copy says NOT_FOUND, the pointer is NULL. */
#define CHECK_PAIR_EMPTY(copy_call, ptr_call)                                  \
    do                                                                         \
    {                                                                          \
        int got = -1;                                                          \
        ASSERT_EQ(SEQC_NOT_FOUND, copy_call(&got));                            \
        ASSERT_EQ(-1, got); /* out untouched */                                \
        ASSERT_NULL(ptr_call);                                                 \
    } while (0)

TEST(vec_get_pair)
{
    vec_t *v = vec_create(sizeof(int), A);
#define COPY(out) vec_get(v, 0, out)
    CHECK_PAIR_EMPTY(COPY, vec_get_ptr(v, 0));
    vec_extend(v, ints(A, ONE_TO_FIVE, 5));
    CHECK_PAIR(COPY, vec_get_ptr(v, 0), 1);
#undef COPY
    ASSERT_EQ(SEQC_NOT_FOUND, vec_get(v, 5, NULL));
    ASSERT_NULL(vec_get_ptr(v, 5));
    ASSERT_EQ(SEQC_INVALID, vec_get(NULL, 0, NULL));
    ASSERT_NULL(vec_get_ptr(NULL, 0));
}

TEST(slice_get_pair)
{
    int data[] = {7, 8, 9};
    slice_t s = {data, 3, sizeof(int)};
#define COPY(out) slice_get(s, 2, out)
    CHECK_PAIR(COPY, slice_get_ptr(s, 2), 9);
#undef COPY
    ASSERT_EQ(SEQC_NOT_FOUND, slice_get(s, 3, NULL));
    ASSERT_NULL(slice_get_ptr(s, 3));
}

TEST(stack_peek_pair)
{
    seqc_stack_t *s = stack_create(sizeof(int), A);
#define COPY(out) stack_peek(s, out)
    CHECK_PAIR_EMPTY(COPY, stack_peek_ptr(s));
    stack_extend(s, ints(A, ONE_TO_FIVE, 5));
    CHECK_PAIR(COPY, stack_peek_ptr(s), 5); /* the top: the last pushed */
#undef COPY
    ASSERT_EQ(SEQC_INVALID, stack_peek(NULL, NULL));
}

TEST(queue_front_back_pairs)
{
    queue_t *q = queue_create(sizeof(int), A);
#define FRONT(out) queue_front(q, out)
#define BACK(out) queue_back(q, out)
    CHECK_PAIR_EMPTY(FRONT, queue_front_ptr(q));
    CHECK_PAIR_EMPTY(BACK, queue_back_ptr(q));
    queue_extend(q, ints(A, ONE_TO_FIVE, 5));
    CHECK_PAIR(FRONT, queue_front_ptr(q), 1);
    CHECK_PAIR(BACK, queue_back_ptr(q), 5);
#undef FRONT
#undef BACK
    ASSERT_EQ(SEQC_INVALID, queue_front(NULL, NULL));
    ASSERT_EQ(SEQC_INVALID, queue_back(NULL, NULL));
}

TEST(list_front_back_pairs)
{
    list_t *l = list_create(sizeof(int), A);
#define FRONT(out) list_front(l, out)
#define BACK(out) list_back(l, out)
    CHECK_PAIR_EMPTY(FRONT, list_front_ptr(l));
    CHECK_PAIR_EMPTY(BACK, list_back_ptr(l));
    list_extend(l, ints(A, ONE_TO_FIVE, 5));
    CHECK_PAIR(FRONT, list_front_ptr(l), 1);
    CHECK_PAIR(BACK, list_back_ptr(l), 5);
#undef FRONT
#undef BACK
    ASSERT_EQ(SEQC_INVALID, list_front(NULL, NULL));
}

TEST(dlist_front_back_pairs)
{
    dlist_t *l = dlist_create(sizeof(int), A);
#define FRONT(out) dlist_front(l, out)
#define BACK(out) dlist_back(l, out)
    CHECK_PAIR_EMPTY(FRONT, dlist_front_ptr(l));
    CHECK_PAIR_EMPTY(BACK, dlist_back_ptr(l));
    dlist_extend(l, ints(A, ONE_TO_FIVE, 5));
    CHECK_PAIR(FRONT, dlist_front_ptr(l), 1);
    CHECK_PAIR(BACK, dlist_back_ptr(l), 5);
#undef FRONT
#undef BACK
    ASSERT_EQ(SEQC_INVALID, dlist_back(NULL, NULL));
}

TEST(ringbuf_pairs)
{
    ringbuf_t *r = ringbuf_create(sizeof(int), A);
#define GET(out) ringbuf_get(r, 0, out)
#define FRONT(out) ringbuf_front(r, out)
#define BACK(out) ringbuf_back(r, out)
    CHECK_PAIR_EMPTY(GET, ringbuf_get_ptr(r, 0));
    CHECK_PAIR_EMPTY(FRONT, ringbuf_front_ptr(r));
    CHECK_PAIR_EMPTY(BACK, ringbuf_back_ptr(r));
    ringbuf_extend(r, ints(A, ONE_TO_FIVE, 5));
    int zero = 0;
    ringbuf_push_front(r, &zero); /* wrap around: front is now index 0 */
    CHECK_PAIR(GET, ringbuf_get_ptr(r, 0), 0);
    CHECK_PAIR(FRONT, ringbuf_front_ptr(r), 0);
    CHECK_PAIR(BACK, ringbuf_back_ptr(r), 5);
#undef GET
#undef FRONT
#undef BACK
    ASSERT_EQ(SEQC_INVALID, ringbuf_get(NULL, 0, NULL));
    ASSERT_NULL(ringbuf_back_ptr(NULL));
}

TEST(pqueue_peek_pair)
{
    pqueue_t *q = pqueue_create(sizeof(int), int_cmp, A);
#define COPY(out) pqueue_peek(q, out)
    CHECK_PAIR_EMPTY(COPY, pqueue_peek_ptr(q));
    const int mixed[] = {4, 1, 5, 2, 3};
    pqueue_extend(q, ints(A, mixed, 5));
    CHECK_PAIR(COPY, pqueue_peek_ptr(q), 1); /* the minimum */
#undef COPY
    ASSERT_EQ(SEQC_INVALID, pqueue_peek(NULL, NULL));
}

TEST(tree_min_max_pairs)
{
    avl_t *a = avl_create(sizeof(int), int_cmp, A);
    bstree_t *b = bstree_create(sizeof(int), int_cmp, A);
#define AMIN(out) avl_min(a, out)
#define AMAX(out) avl_max(a, out)
#define BMIN(out) bstree_min(b, out)
#define BMAX(out) bstree_max(b, out)
    CHECK_PAIR_EMPTY(AMIN, avl_min_ptr(a));
    CHECK_PAIR_EMPTY(BMAX, bstree_max_ptr(b));
    const int mixed[] = {3, 1, 5, 2, 4};
    avl_extend(a, ints(A, mixed, 5));
    bstree_extend(b, ints(A, mixed, 5));
    CHECK_PAIR(AMIN, avl_min_ptr(a), 1);
    CHECK_PAIR(AMAX, avl_max_ptr(a), 5);
    CHECK_PAIR(BMIN, bstree_min_ptr(b), 1);
    CHECK_PAIR(BMAX, bstree_max_ptr(b), 5);
#undef AMIN
#undef AMAX
#undef BMIN
#undef BMAX
    ASSERT_EQ(SEQC_INVALID, avl_min(NULL, NULL));
    ASSERT_EQ(SEQC_INVALID, bstree_max(NULL, NULL));
}

TEST(omap_get_and_key_pairs)
{
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, A);
    int k = 2;
#define GET(out) omap_get(m, &k, out)
#define KMIN(out) omap_min_key(m, out)
#define KMAX(out) omap_max_key(m, out)
    CHECK_PAIR_EMPTY(GET, omap_get_ptr(m, &k));
    CHECK_PAIR_EMPTY(KMIN, omap_min_key_ptr(m));
    for (int i = 1; i <= 3; ++i)
    {
        int v = i * 10;
        omap_set(m, &i, &v);
    }
    CHECK_PAIR(GET, omap_get_ptr(m, &k), 20);
    CHECK_PAIR(KMIN, omap_min_key_ptr(m), 1);
    CHECK_PAIR(KMAX, omap_max_key_ptr(m), 3);
#undef GET
#undef KMIN
#undef KMAX
    ASSERT_EQ(SEQC_INVALID, omap_get(NULL, &k, NULL));
    ASSERT_EQ(SEQC_INVALID, omap_get(m, NULL, NULL));
}

TEST(hashmap_get_pair)
{
    hashmap_t *m =
        hashmap_create(sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, A);
    int k = 7;
#define GET(out) hashmap_get(m, &k, out)
    CHECK_PAIR_EMPTY(GET, hashmap_get_ptr(m, &k));
    int v = 70;
    hashmap_set(m, &k, &v);
    CHECK_PAIR(GET, hashmap_get_ptr(m, &k), 70);
#undef GET
    ASSERT_EQ(SEQC_INVALID, hashmap_get(NULL, &k, NULL));
    ASSERT_EQ(SEQC_INVALID, hashmap_get(m, NULL, NULL));
}

/* What the pointers are for: changing a value in place. */
TEST(ptr_accessors_update_in_place)
{
    hashmap_t *counts =
        hashmap_create(sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, A);
    int word = 42, zero = 0;
    hashmap_set(counts, &word, &zero);
    for (int i = 0; i < 3; ++i)
    {
        (*(int *)hashmap_get_ptr(counts, &word))++;
    }
    int n = 0;
    hashmap_get(counts, &word, &n);
    ASSERT_EQ(3, n);

    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, A);
    omap_set(m, &word, &zero);
    *(int *)omap_get_ptr(m, &word) = 9;
    omap_get(m, &word, &n);
    ASSERT_EQ(9, n);

    vec_t *v = vec_create(sizeof(int), A);
    vec_push(v, &zero);
    *(int *)vec_get_ptr(v, 0) = 5;
    vec_get(v, 0, &n);
    ASSERT_EQ(5, n);
}

/* What the copies are for: the value survives changes that move entries.
 * Hundreds of inserts rehash the map; the copy is unaffected. */
TEST(copies_survive_later_changes)
{
    hashmap_t *m =
        hashmap_create(sizeof(int), sizeof(int), hash_fnv1a, hash_eq_bytes, A);
    int k = 1, v = 100;
    hashmap_set(m, &k, &v);
    int copy = 0;
    ASSERT_EQ(SEQC_OK, hashmap_get(m, &k, &copy));
    for (int i = 2; i < 500; ++i)
    {
        hashmap_set(m, &i, &i);
    }
    ASSERT_EQ(100, copy);
    ASSERT_EQ(SEQC_OK, hashmap_get(m, &k, &copy));
    ASSERT_EQ(100, copy); /* and the map still has it */
}

/* --- is_empty ------------------------------------------------------------- */

TEST(is_empty_on_every_collection)
{
    vec_t *v = vec_create(sizeof(int), A);
    avl_t *a = avl_create(sizeof(int), int_cmp, A);
    bstree_t *b = bstree_create(sizeof(int), int_cmp, A);
    omap_t *m = omap_create(sizeof(int), sizeof(int), int_cmp, A);
    strbuf_t *sb = strbuf_create(A);

    ASSERT_TRUE(vec_is_empty(NULL));
    ASSERT_TRUE(avl_is_empty(NULL));
    ASSERT_TRUE(bstree_is_empty(NULL));
    ASSERT_TRUE(omap_is_empty(NULL));
    ASSERT_TRUE(strbuf_is_empty(NULL));

    ASSERT_TRUE(vec_is_empty(v));
    ASSERT_TRUE(avl_is_empty(a));
    ASSERT_TRUE(bstree_is_empty(b));
    ASSERT_TRUE(omap_is_empty(m));
    ASSERT_TRUE(strbuf_is_empty(sb));

    int one = 1;
    vec_push(v, &one);
    avl_add(a, &one);
    bstree_add(b, &one);
    omap_set(m, &one, &one);
    strbuf_append_char(sb, 'x');

    ASSERT_FALSE(vec_is_empty(v));
    ASSERT_FALSE(avl_is_empty(a));
    ASSERT_FALSE(bstree_is_empty(b));
    ASSERT_FALSE(omap_is_empty(m));
    ASSERT_FALSE(strbuf_is_empty(sb));

    vec_clear(v);
    avl_clear(a);
    bstree_clear(b);
    omap_clear(m);
    strbuf_clear(sb);

    ASSERT_TRUE(vec_is_empty(v));
    ASSERT_TRUE(avl_is_empty(a));
    ASSERT_TRUE(bstree_is_empty(b));
    ASSERT_TRUE(omap_is_empty(m));
    ASSERT_TRUE(strbuf_is_empty(sb));
}

/* --- extend -----------------------------------------------------------------
 */

TEST(extend_keeps_order_for_sequences)
{
    list_t *l = list_create(sizeof(int), A);
    ASSERT_EQ(SEQC_OK, list_extend(l, ints(A, ONE_TO_FIVE, 5)));
    ASSERT_EQ(5, list_len(l));
    int x = 0;
    for (int expect = 1; expect <= 5; ++expect)
    {
        list_pop_front(l, &x);
        ASSERT_EQ(expect, x);
    }

    ringbuf_t *r = ringbuf_create(sizeof(int), A);
    ASSERT_EQ(SEQC_OK, ringbuf_extend(r, ints(A, ONE_TO_FIVE, 5)));
    for (int expect = 1; expect <= 5; ++expect)
    {
        ringbuf_pop_front(r, &x);
        ASSERT_EQ(expect, x);
    }

    seqc_stack_t *s = stack_create(sizeof(int), A);
    ASSERT_EQ(SEQC_OK, stack_extend(s, ints(A, ONE_TO_FIVE, 5)));
    for (int expect = 5; expect >= 1; --expect) /* LIFO */
    {
        stack_pop(s, &x);
        ASSERT_EQ(expect, x);
    }
}

TEST(extend_into_sets_skips_duplicates)
{
    const int dups[] = {3, 1, 3, 2, 1};
    avl_t *a = avl_create(sizeof(int), int_cmp, A);
    bstree_t *b = bstree_create(sizeof(int), int_cmp, A);
    ASSERT_EQ(SEQC_OK, avl_extend(a, ints(A, dups, 5)));
    ASSERT_EQ(SEQC_OK, bstree_extend(b, ints(A, dups, 5)));
    ASSERT_EQ(3, avl_len(a));
    ASSERT_EQ(3, bstree_len(b));
}

TEST(extend_pqueue_orders_by_priority)
{
    const int mixed[] = {4, 1, 5, 2, 3};
    pqueue_t *q = pqueue_create(sizeof(int), int_cmp, A);
    ASSERT_EQ(SEQC_OK, pqueue_extend(q, ints(A, mixed, 5)));
    int x = 0;
    for (int expect = 1; expect <= 5; ++expect)
    {
        pqueue_pop(q, &x);
        ASSERT_EQ(expect, x);
    }
}

TEST(extend_omap_from_another_map)
{
    omap_t *src = omap_create(sizeof(int), sizeof(int), int_cmp, A);
    for (int i = 1; i <= 4; ++i)
    {
        int v = i * i;
        omap_set(src, &i, &v);
    }
    omap_t *dst = omap_create(sizeof(int), sizeof(int), int_cmp, A);
    ASSERT_EQ(SEQC_OK, omap_extend(dst, omap_iter(src)));
    ASSERT_EQ(4, omap_len(dst));
    int k = 3, v = 0;
    ASSERT_EQ(SEQC_OK, omap_get(dst, &k, &v));
    ASSERT_EQ(9, v);
}

/* A NULL collection is INVALID — and the iterator is still consumed.  The
 * iterator's state comes from malloc here: LeakSanitizer would report it
 * if extend forgot to destroy it. */
TEST(extend_on_null_consumes_the_iterator)
{
    allocator_t heap = sys_allocator();
    vec_t *backing = vec_create(sizeof(int), heap);
    vec_extend(backing, ints(A, ONE_TO_FIVE, 5));

    ASSERT_EQ(SEQC_INVALID, stack_extend(NULL, vec_iter(backing)));
    ASSERT_EQ(SEQC_INVALID, queue_extend(NULL, vec_iter(backing)));
    ASSERT_EQ(SEQC_INVALID, list_extend(NULL, vec_iter(backing)));
    ASSERT_EQ(SEQC_INVALID, dlist_extend(NULL, vec_iter(backing)));
    ASSERT_EQ(SEQC_INVALID, ringbuf_extend(NULL, vec_iter(backing)));
    ASSERT_EQ(SEQC_INVALID, pqueue_extend(NULL, vec_iter(backing)));
    ASSERT_EQ(SEQC_INVALID, avl_extend(NULL, vec_iter(backing)));
    ASSERT_EQ(SEQC_INVALID, bstree_extend(NULL, vec_iter(backing)));
    ASSERT_EQ(SEQC_INVALID, omap_extend(NULL, vec_iter(backing)));

    vec_destroy(backing);
}

/* --- the renamed helpers ----------------------------------------------------
 */

TEST(hash_cstr_hashes_content_not_pointers)
{
    char a[] = "readme", b[] = "readme";
    const char *pa = a, *pb = b;
    ASSERT_TRUE(pa != pb);
    ASSERT_EQ(hash_cstr(&pa, sizeof pa), hash_cstr(&pb, sizeof pb));
    ASSERT_TRUE(hash_eq_cstr(&pa, &pb, sizeof pa));

    hashmap_t *m = hashmap_create(
        sizeof(const char *), sizeof(int), hash_cstr, hash_eq_cstr, A);
    int v = 1, out = 0;
    hashmap_set(m, &pa, &v);
    ASSERT_EQ(SEQC_OK, hashmap_get(m, &pb, &out)); /* found via the copy */
    ASSERT_EQ(1, out);
}

static bool is_even(const void *e, void *ctx)
{
    (void)ctx;
    return *(const int *)e % 2 == 0;
}

static bool is_negative(const void *e, void *ctx)
{
    (void)ctx;
    return *(const int *)e < 0;
}

TEST(vec_any_and_slice_any)
{
    vec_t *v = vec_create(sizeof(int), A);
    vec_extend(v, ints(A, ONE_TO_FIVE, 5));
    ASSERT_TRUE(vec_any(v, is_even, NULL));
    ASSERT_FALSE(vec_any(v, is_negative, NULL));
    ASSERT_TRUE(slice_any(vec_as_slice(v), is_even, NULL));
    ASSERT_FALSE(slice_any(vec_as_slice(v), is_negative, NULL));
}

TEST(vec_create_with_cap_reserves)
{
    vec_t *v = vec_create_with_cap(sizeof(int), 64, A);
    ASSERT_GE(vec_cap(v), 64);
    ASSERT_EQ(0, vec_len(v));
    ASSERT_TRUE(vec_is_empty(v));
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "seqc api conventions");
}
