#include "seqc/set.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define SET_INITIAL_CAP 16

typedef struct
{
    void *key;
    uint8_t psl; /* probe-sequence length; 0 = empty */
} set_bucket_t;

struct set_t
{
    set_bucket_t *buckets;
    size_t cap; /* always a power of 2 */
    size_t len;
    size_t elem_size;
    uint8_t
        max_psl; /* highest PSL of any stored bucket; updated on every insert */
    hash_fn hash;
    eq_fn eq;
    allocator_t allocator;
};

/* ---- internal helpers -------------------------------------------------- */

static size_t set_slot(const set_t *s, const void *key)
{
    return s->hash(key, s->elem_size) & (s->cap - 1);
}

/* Insert a bucket whose key is already allocated.  Does not touch s->len.
 * Returns true if inserted, false if a duplicate was found (incoming.key
 * is freed on duplicate so the caller does not need to). */
static bool set_insert_raw(set_t *s, set_bucket_t incoming)
{
    size_t slot = set_slot(s, incoming.key);
    incoming.psl = 1;
    for (;;)
    {
        set_bucket_t *cur = &s->buckets[slot];
        if (cur->psl == 0)
        {
            *cur = incoming;
            if (incoming.psl > s->max_psl)
                s->max_psl = incoming.psl;
            return true;
        }
        if (s->eq(cur->key, incoming.key, s->elem_size))
        {
            mem_free(s->allocator, incoming.key, s->elem_size);
            return false; /* duplicate */
        }
        /* Robin Hood: steal the slot from the "rich" (low psl) bucket */
        if (cur->psl < incoming.psl)
        {
            set_bucket_t tmp = *cur;
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

static seqc_status_t set_resize(set_t *s)
{
    size_t new_cap = s->cap * 2;
    set_bucket_t *nb = mem_alloc(
        s->allocator, new_cap * sizeof(set_bucket_t), _Alignof(set_bucket_t));
    if (!nb)
        return SEQC_OOM;
    memset(nb, 0, new_cap * sizeof(set_bucket_t));

    set_bucket_t *old = s->buckets;
    size_t old_cap = s->cap;
    s->buckets = nb;
    s->cap = new_cap;
    s->max_psl = 0; /* recalculated during re-insertion below */

    for (size_t i = 0; i < old_cap; i++)
        if (old[i].psl > 0)
            set_insert_raw(s, (set_bucket_t){old[i].key, 0});

    mem_free(s->allocator, old, old_cap * sizeof(set_bucket_t));
    return SEQC_OK;
}

/* ---- public API -------------------------------------------------------- */

set_t *set_create(size_t elem_size, hash_fn hash, eq_fn eq, allocator_t allocator)
{
    set_t *s = mem_alloc(allocator, sizeof(set_t), _Alignof(set_t));
    if (!s)
        return NULL;
    *s = (set_t){.buckets = NULL,
               .cap = 0,
               .len = 0,
               .elem_size = elem_size,
               .hash = hash,
               .eq = eq,
               .allocator = allocator};
    return s;
}

bool set_contains(const set_t *s, const void *elem)
{
    if (!s || !elem || s->len == 0)
        return false;
    size_t slot = set_slot(s, elem);
    uint8_t probe = 1;
    while (1)
    {
        const set_bucket_t *b = &s->buckets[slot];
        if (b->psl == 0 || b->psl < probe)
            return false;
        if (s->eq(b->key, elem, s->elem_size))
            return true;
        slot = (slot + 1) & (s->cap - 1);
        probe++;
    }
}

seqc_status_t set_add(set_t *s, const void *elem)
{
    if (!s || !elem)
        return SEQC_INVALID;
    if (s->cap == 0)
    {
        s->buckets = mem_alloc(
            s->allocator,

            SET_INITIAL_CAP * sizeof(set_bucket_t),
            _Alignof(set_bucket_t));
        if (!s->buckets)
            return SEQC_OOM;
        memset(s->buckets, 0, SET_INITIAL_CAP * sizeof(set_bucket_t));
        s->cap = SET_INITIAL_CAP;
    }
    if (s->len * 4 >= s->cap * 3 || s->max_psl >= SET_PSL_THRESHOLD)
    {
        seqc_status_t rs = set_resize(s);
        if (rs != SEQC_OK)
            return rs;
    }
    void *key = mem_alloc(s->allocator, s->elem_size, _Alignof(max_align_t));
    if (!key)
        return SEQC_OOM;
    memcpy(key, elem, s->elem_size);
    if (!set_insert_raw(s, (set_bucket_t){key, 0}))
        return SEQC_DUPLICATE; /* key freed inside set_insert_raw */
    s->len++;
    return SEQC_OK;
}

seqc_status_t set_remove(set_t *s, const void *elem)
{
    if (!s || !elem || s->len == 0)
        return SEQC_NOT_FOUND;
    size_t slot = set_slot(s, elem);
    uint8_t probe = 1;
    while (1)
    {
        set_bucket_t *b = &s->buckets[slot];
        if (b->psl == 0 || b->psl < probe)
            return SEQC_NOT_FOUND;
        if (s->eq(b->key, elem, s->elem_size))
        {
            mem_free(s->allocator, b->key, s->elem_size);
            /* backward shift to restore Robin Hood invariant */
            for (;;)
            {
                size_t next = (slot + 1) & (s->cap - 1);
                set_bucket_t *nb = &s->buckets[next];
                if (nb->psl <= 1)
                {
                    s->buckets[slot] = (set_bucket_t){NULL, 0};
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

size_t set_len(const set_t *s)
{
    return s ? s->len : 0;
}

bool set_is_empty(const set_t *s)
{
    return set_len(s) == 0;
}

void set_free(set_t *s)
{
    if (!s)
        return;
    if (s->buckets) /* may be NULL: empty set, or first add OOM'd */
    {
        for (size_t i = 0; i < s->cap; i++)
            if (s->buckets[i].psl > 0)
                mem_free(s->allocator, s->buckets[i].key, s->elem_size);
        mem_free(s->allocator, s->buckets, s->cap * sizeof(set_bucket_t));
    }
    allocator_t al = s->allocator;
    mem_free(al, s, sizeof(set_t));
}

void set_clear(set_t *s)
{
    if (!s || !s->buckets)
        return;
    for (size_t i = 0; i < s->cap; i++)
        if (s->buckets[i].psl > 0)
            mem_free(s->allocator, s->buckets[i].key, s->elem_size);
    memset(s->buckets, 0, s->cap * sizeof(set_bucket_t));
    s->len = 0;
    s->max_psl = 0;
}

/* ---- health / diagnostics ---------------------------------------------- */

bool set_is_healthy(const set_t *s)
{
    return s && s->max_psl <= SET_PSL_THRESHOLD / 2;
}

set_stats_t set_audit(const set_t *s)
{
    if (!s)
        return (set_stats_t){0};
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
    return (set_stats_t){
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
    const set_bucket_t *buckets;
    size_t cap;
    size_t pos;
    size_t elem_size;
} set_iter_state_t;

static bool set_iter_next(iter_t *it, void *out)
{
    set_iter_state_t *s = it->state;
    while (s->pos < s->cap)
    {
        const set_bucket_t *b = &s->buckets[s->pos++];
        if (b->psl > 0)
        {
            memcpy(out, b->key, s->elem_size);
            return true;
        }
    }
    return false;
}

static void set_iter_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(set_iter_state_t));
}

iter_t set_iter(const set_t *s)
{
    if (!s)
        return (iter_t){0};
    set_iter_state_t *state =
        mem_alloc(s->allocator, sizeof *state, _Alignof(set_iter_state_t));
    if (!state)
        return (iter_t){0};
    *state = (set_iter_state_t){s->buckets, s->cap, 0, s->elem_size};
    return (iter_t){.next = set_iter_next,
                  .drop = set_iter_drop,
                  .state = state,
                  .elem_size = s->elem_size,
                  .allocator = s->allocator};
}

typedef struct
{
    const set_bucket_t *buckets;
    size_t cap;
    size_t pos; /* counts down from cap */
    size_t elem_size;
} set_iter_rev_state_t;

static bool set_iter_rev_next(iter_t *it, void *out)
{
    set_iter_rev_state_t *s = it->state;
    while (s->pos > 0)
    {
        const set_bucket_t *b = &s->buckets[--s->pos];
        if (b->psl > 0)
        {
            memcpy(out, b->key, s->elem_size);
            return true;
        }
    }
    return false;
}

static void set_iter_rev_drop(iter_t *it)
{
    mem_free(it->allocator, it->state, sizeof(set_iter_rev_state_t));
}

iter_t set_iter_rev(const set_t *s)
{
    if (!s)
        return (iter_t){0};
    set_iter_rev_state_t *state =
        mem_alloc(s->allocator, sizeof *state, _Alignof(set_iter_rev_state_t));
    if (!state)
        return (iter_t){0};
    *state = (set_iter_rev_state_t){s->buckets, s->cap, s->cap, s->elem_size};
    return (iter_t){.next = set_iter_rev_next,
                  .drop = set_iter_rev_drop,
                  .state = state,
                  .elem_size = s->elem_size,
                  .allocator = s->allocator};
}

/* ---- set algebra ------------------------------------------------------- */

seqc_status_t set_union(set_t *dest, const set_t *a, const set_t *b)
{
    if (!dest || !a || !b)
        return SEQC_INVALID;
    /* add all elements from a */
    for (size_t i = 0; i < a->cap; i++)
    {
        if (a->buckets[i].psl == 0)
            continue;
        seqc_status_t st = set_add(dest, a->buckets[i].key);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            return st;
    }
    /* add all elements from b (duplicates are fine) */
    for (size_t i = 0; i < b->cap; i++)
    {
        if (b->buckets[i].psl == 0)
            continue;
        seqc_status_t st = set_add(dest, b->buckets[i].key);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            return st;
    }
    return SEQC_OK;
}

seqc_status_t set_intersection(set_t *dest, const set_t *a, const set_t *b)
{
    if (!dest || !a || !b)
        return SEQC_INVALID;
    /* iterate the smaller set for efficiency */
    const set_t *src = a->len <= b->len ? a : b;
    const set_t *other = a->len <= b->len ? b : a;
    for (size_t i = 0; i < src->cap; i++)
    {
        if (src->buckets[i].psl == 0)
            continue;
        if (!set_contains(other, src->buckets[i].key))
            continue;
        seqc_status_t st = set_add(dest, src->buckets[i].key);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            return st;
    }
    return SEQC_OK;
}

seqc_status_t set_difference(set_t *dest, const set_t *a, const set_t *b)
{
    if (!dest || !a || !b)
        return SEQC_INVALID;
    for (size_t i = 0; i < a->cap; i++)
    {
        if (a->buckets[i].psl == 0)
            continue;
        if (set_contains(b, a->buckets[i].key))
            continue;
        seqc_status_t st = set_add(dest, a->buckets[i].key);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            return st;
    }
    return SEQC_OK;
}

seqc_status_t set_add_all(set_t *s, iter_t it)
{
    if (!s)
    {
        iter_drop(&it);
        return SEQC_INVALID;
    }
    void *elem = mem_alloc(s->allocator, s->elem_size, _Alignof(max_align_t));
    if (!elem)
    {
        iter_drop(&it);
        return SEQC_OOM;
    }
    seqc_status_t st = SEQC_OK;
    while (it.next(&it, elem))
    {
        st = set_add(s, elem);
        if (st != SEQC_OK && st != SEQC_DUPLICATE)
            break;
        st = SEQC_OK;
    }
    iter_drop(&it);
    mem_free(s->allocator, elem, s->elem_size);
    return st;
}
