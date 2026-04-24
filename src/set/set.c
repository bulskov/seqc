#include "seqc/set.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define SET_INITIAL_CAP 16

typedef struct
{
    void *key;
    uint8_t psl; /* probe-sequence length; 0 = empty */
} SetBucket;

struct Set
{
    SetBucket *buckets;
    size_t cap; /* always a power of 2 */
    size_t len;
    size_t elem_size;
    uint8_t
        max_psl; /* highest PSL of any stored bucket; updated on every insert */
    hash_fn hash;
    eq_fn eq;
    Allocator allocator;
};

/* ---- internal helpers -------------------------------------------------- */

static size_t set_slot(const Set *s, const void *key)
{
    return s->hash(key, s->elem_size) & (s->cap - 1);
}

/* Insert a bucket whose key is already allocated.  Does not touch s->len.
 * Returns true if inserted, false if a duplicate was found (incoming.key
 * is freed on duplicate so the caller does not need to). */
static bool set_insert_raw(Set *s, SetBucket incoming)
{
    size_t slot = set_slot(s, incoming.key);
    incoming.psl = 1;
    for (;;)
    {
        SetBucket *cur = &s->buckets[slot];
        if (cur->psl == 0)
        {
            *cur = incoming;
            if (incoming.psl > s->max_psl)
                s->max_psl = incoming.psl;
            return true;
        }
        if (s->eq(cur->key, incoming.key, s->elem_size))
        {
            if (s->allocator.free)
                s->allocator.free(s->allocator.ctx, incoming.key);
            return false; /* duplicate */
        }
        /* Robin Hood: steal the slot from the "rich" (low psl) bucket */
        if (cur->psl < incoming.psl)
        {
            SetBucket tmp = *cur;
            *cur = incoming;
            if (incoming.psl > s->max_psl)
                s->max_psl = incoming.psl;
            incoming = tmp;
        }
        incoming.psl++;
        assert(
            incoming.psl != 0); /* uint8_t overflow: degenerate hash function */
        slot = (slot + 1) & (s->cap - 1);
    }
}

static SeqcStatus set_resize(Set *s)
{
    size_t new_cap = s->cap * 2;
    SetBucket *nb = s->allocator.alloc(
        s->allocator.ctx, new_cap * sizeof(SetBucket), _Alignof(SetBucket));
    if (!nb)
        return SEQC_OOM;
    memset(nb, 0, new_cap * sizeof(SetBucket));

    SetBucket *old = s->buckets;
    size_t old_cap = s->cap;
    s->buckets = nb;
    s->cap = new_cap;
    s->max_psl = 0; /* recalculated during re-insertion below */

    for (size_t i = 0; i < old_cap; i++)
        if (old[i].psl > 0)
            set_insert_raw(s, (SetBucket){old[i].key, 0});

    if (s->allocator.free)
        s->allocator.free(s->allocator.ctx, old);
    return SEQC_OK;
}

/* ---- public API -------------------------------------------------------- */

Set *set_create(size_t elem_size, hash_fn hash, eq_fn eq, Allocator allocator)
{
    Set *s = allocator.alloc(allocator.ctx, sizeof(Set), _Alignof(Set));
    if (!s)
        return NULL;
    *s = (Set){.buckets = NULL,
               .cap = 0,
               .len = 0,
               .elem_size = elem_size,
               .hash = hash,
               .eq = eq,
               .allocator = allocator};
    return s;
}

bool set_contains(const Set *s, const void *elem)
{
    if (!s || !elem || s->len == 0)
        return false;
    size_t slot = set_slot(s, elem);
    uint8_t probe = 1;
    while (1)
    {
        const SetBucket *b = &s->buckets[slot];
        if (b->psl == 0 || b->psl < probe)
            return false;
        if (s->eq(b->key, elem, s->elem_size))
            return true;
        slot = (slot + 1) & (s->cap - 1);
        probe++;
    }
}

SeqcStatus set_add(Set *s, const void *elem)
{
    if (!s || !elem)
        return SEQC_INVALID;
    if (s->cap == 0)
    {
        s->buckets = s->allocator.alloc(
            s->allocator.ctx,
            SET_INITIAL_CAP * sizeof(SetBucket),
            _Alignof(SetBucket));
        if (!s->buckets)
            return SEQC_OOM;
        memset(s->buckets, 0, SET_INITIAL_CAP * sizeof(SetBucket));
        s->cap = SET_INITIAL_CAP;
    }
    if (s->len * 4 >= s->cap * 3 || s->max_psl >= SET_PSL_THRESHOLD)
    {
        SeqcStatus rs = set_resize(s);
        if (rs != SEQC_OK)
            return rs;
    }
    void *key = s->allocator.alloc(
        s->allocator.ctx, s->elem_size, _Alignof(max_align_t));
    if (!key)
        return SEQC_OOM;
    memcpy(key, elem, s->elem_size);
    if (!set_insert_raw(s, (SetBucket){key, 0}))
        return SEQC_DUPLICATE; /* key freed inside set_insert_raw */
    s->len++;
    return SEQC_OK;
}

SeqcStatus set_remove(Set *s, const void *elem)
{
    if (!s || !elem || s->len == 0)
        return SEQC_NOT_FOUND;
    size_t slot = set_slot(s, elem);
    uint8_t probe = 1;
    while (1)
    {
        SetBucket *b = &s->buckets[slot];
        if (b->psl == 0 || b->psl < probe)
            return SEQC_NOT_FOUND;
        if (s->eq(b->key, elem, s->elem_size))
        {
            if (s->allocator.free)
                s->allocator.free(s->allocator.ctx, b->key);
            /* backward shift to restore Robin Hood invariant */
            for (;;)
            {
                size_t next = (slot + 1) & (s->cap - 1);
                SetBucket *nb = &s->buckets[next];
                if (nb->psl <= 1)
                {
                    s->buckets[slot] = (SetBucket){NULL, 0};
                    break;
                }
                s->buckets[slot] = *nb;
                s->buckets[slot].psl--;
                slot = next;
            }
            s->len--;
            return SEQC_OK;
        }
        slot = (slot + 1) & (s->cap - 1);
        probe++;
    }
}

size_t set_len(const Set *s)
{
    return s ? s->len : 0;
}

bool set_is_empty(const Set *s)
{
    return set_len(s) == 0;
}

void set_free(Set *s)
{
    if (!s || !s->buckets)
        return;
    if (s->allocator.free)
    {
        for (size_t i = 0; i < s->cap; i++)
            if (s->buckets[i].psl > 0)
                s->allocator.free(s->allocator.ctx, s->buckets[i].key);
        s->allocator.free(s->allocator.ctx, s->buckets);
    }
    Allocator al = s->allocator;
    if (al.free)
        al.free(al.ctx, s);
}

void set_clear(Set *s)
{
    if (!s || !s->buckets)
        return;
    if (s->allocator.free)
    {
        for (size_t i = 0; i < s->cap; i++)
            if (s->buckets[i].psl > 0)
                s->allocator.free(s->allocator.ctx, s->buckets[i].key);
    }
    memset(s->buckets, 0, s->cap * sizeof(SetBucket));
    s->len = 0;
    s->max_psl = 0;
}

/* ---- health / diagnostics ---------------------------------------------- */

bool set_is_healthy(const Set *s)
{
    return s && s->max_psl <= SET_PSL_THRESHOLD / 2;
}

SetStats set_audit(const Set *s)
{
    if (!s)
        return (SetStats){0};
    double sum_psl = 0;
    uint8_t max_psl = 0;
    for (size_t i = 0; i < s->cap; i++)
    {
        uint8_t p = s->buckets[i].psl;
        if (p != 0)
        {
            sum_psl += p;
            if (p > max_psl)
                max_psl = p;
        }
    }
    double load_factor = s->cap > 0 ? (double)s->len / s->cap : 0.0;
    double mean_psl = s->len > 0 ? sum_psl / (double)s->len : 0.0;
    return (SetStats){
        .len = s->len,
        .cap = s->cap,
        .load_factor = load_factor,
        .max_psl = max_psl,
        .mean_psl = mean_psl,
        .is_healthy = mean_psl < 3.0 && max_psl <= SET_PSL_THRESHOLD / 2,
    };
}

/* ---- iter -------------------------------------------------------------- */

typedef struct
{
    const SetBucket *buckets;
    size_t cap;
    size_t pos;
    size_t elem_size;
} SetIterState;

static bool set_iter_next(Iter *it, void *out)
{
    SetIterState *s = it->state;
    while (s->pos < s->cap)
    {
        const SetBucket *b = &s->buckets[s->pos++];
        if (b->psl > 0)
        {
            memcpy(out, b->key, s->elem_size);
            return true;
        }
    }
    return false;
}

static void set_iter_drop(Iter *it)
{
    if (it->allocator.free)
        it->allocator.free(it->allocator.ctx, it->state);
}

Iter set_iter(const Set *s)
{
    if (!s)
        return (Iter){0};
    SetIterState *state = s->allocator.alloc(
        s->allocator.ctx, sizeof *state, _Alignof(SetIterState));
    if (!state)
        return (Iter){0};
    *state = (SetIterState){s->buckets, s->cap, 0, s->elem_size};
    return (Iter){.next = set_iter_next,
                  .drop = set_iter_drop,
                  .state = state,
                  .elem_size = s->elem_size,
                  .allocator = s->allocator};
}

typedef struct
{
    const SetBucket *buckets;
    size_t cap;
    size_t pos; /* counts down from cap */
    size_t elem_size;
} SetIterRevState;

static bool set_iter_rev_next(Iter *it, void *out)
{
    SetIterRevState *s = it->state;
    while (s->pos > 0)
    {
        const SetBucket *b = &s->buckets[--s->pos];
        if (b->psl > 0)
        {
            memcpy(out, b->key, s->elem_size);
            return true;
        }
    }
    return false;
}

static void set_iter_rev_drop(Iter *it)
{
    if (it->allocator.free)
        it->allocator.free(it->allocator.ctx, it->state);
}

Iter set_iter_rev(const Set *s)
{
    if (!s)
        return (Iter){0};
    SetIterRevState *state = s->allocator.alloc(
        s->allocator.ctx, sizeof *state, _Alignof(SetIterRevState));
    if (!state)
        return (Iter){0};
    *state = (SetIterRevState){s->buckets, s->cap, s->cap, s->elem_size};
    return (Iter){.next = set_iter_rev_next,
                  .drop = set_iter_rev_drop,
                  .state = state,
                  .elem_size = s->elem_size,
                  .allocator = s->allocator};
}

/* ---- set algebra ------------------------------------------------------- */

SeqcStatus set_union(Set *dest, const Set *a, const Set *b)
{
    if (!dest || !a || !b)
        return SEQC_INVALID;
    /* add all elements from a */
    for (size_t i = 0; i < a->cap; i++)
    {
        if (a->buckets[i].psl == 0)
            continue;
        SeqcStatus st = set_add(dest, a->buckets[i].key);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            return st;
    }
    /* add all elements from b (duplicates are fine) */
    for (size_t i = 0; i < b->cap; i++)
    {
        if (b->buckets[i].psl == 0)
            continue;
        SeqcStatus st = set_add(dest, b->buckets[i].key);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            return st;
    }
    return SEQC_OK;
}

SeqcStatus set_intersection(Set *dest, const Set *a, const Set *b)
{
    if (!dest || !a || !b)
        return SEQC_INVALID;
    /* iterate the smaller set for efficiency */
    const Set *src = a->len <= b->len ? a : b;
    const Set *other = a->len <= b->len ? b : a;
    for (size_t i = 0; i < src->cap; i++)
    {
        if (src->buckets[i].psl == 0)
            continue;
        if (!set_contains(other, src->buckets[i].key))
            continue;
        SeqcStatus st = set_add(dest, src->buckets[i].key);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            return st;
    }
    return SEQC_OK;
}

SeqcStatus set_difference(Set *dest, const Set *a, const Set *b)
{
    if (!dest || !a || !b)
        return SEQC_INVALID;
    for (size_t i = 0; i < a->cap; i++)
    {
        if (a->buckets[i].psl == 0)
            continue;
        if (set_contains(b, a->buckets[i].key))
            continue;
        SeqcStatus st = set_add(dest, a->buckets[i].key);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            return st;
    }
    return SEQC_OK;
}

SeqcStatus set_add_all(Set *s, Iter it)
{
    if (!s)
    {
        iter_drop(&it);
        return SEQC_INVALID;
    }
    void *elem = s->allocator.alloc(
        s->allocator.ctx, s->elem_size, _Alignof(max_align_t));
    if (!elem)
    {
        iter_drop(&it);
        return SEQC_OOM;
    }
    SeqcStatus st = SEQC_OK;
    while (it.next(&it, elem))
    {
        st = set_add(s, elem);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            break;
        st = SEQC_OK;
    }
    iter_drop(&it);
    if (s->allocator.free)
        s->allocator.free(s->allocator.ctx, elem);
    return st;
}
