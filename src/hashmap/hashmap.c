#include "seqc/hashmap.h"
#include "seqc/hash.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    void *key;
    void *value;
    uint8_t psl; /* 0 = empty */
} Bucket;

struct HashMap
{
    Bucket *buckets;
    size_t cap; /* always power of 2 */
    size_t len;
    size_t key_size;
    size_t val_size;
    uint8_t
        max_psl; /* highest PSL of any stored bucket; updated on every insert */
    hash_fn hash;
    eq_fn eq;
    Allocator allocator;
};

size_t hash_fnv1a(const void *key, size_t key_size)
{
    if (key_size == 0 || key == NULL)
    {
        return 0;
    }
    const uint8_t *data = (const uint8_t *)key;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < key_size; i++)
    {
        hash ^= (uint64_t)data[i];
        hash *= 1099511628211ULL;
    }
    return (size_t)hash;
}

bool hash_eq_bytes(const void *a, const void *b, size_t key_size)
{
    if (key_size == 0)
        return true; /* treat all zero-size keys as equal */
    if (a == NULL || b == NULL)
        return false; /* treat null pointers as unequal to any key, including
                       * each other
                       */
    return memcmp(a, b, key_size) == 0;
}

size_t hash_fnv1a_str(const void *key, size_t key_size)
{
    if (key_size != sizeof(char *) || key == NULL)
    {
        return 0; /* invalid key size for string map */
    }
    const char *s = *(const char *const *)key;
    if (!s)
        return 0;
    uint64_t hash = 14695981039346656037ULL;
    for (; *s; s++)
    {
        hash ^= (uint64_t)(unsigned char)*s;
        hash *= 1099511628211ULL;
    }
    return (size_t)hash;
}

bool hash_eq_str(const void *a, const void *b, size_t key_size)
{
    if (key_size != sizeof(char *) || a == NULL || b == NULL)
    {
        return false; /* invalid key size for string map */
    }
    return strcmp(*(const char *const *)a, *(const char *const *)b) == 0;
}

static size_t hashmap_get_slot(const HashMap *map, const void *key)
{
    size_t h = map->hash(key, map->key_size);
    return h & (map->cap - 1);
}

static void hashmap_insert_raw(HashMap *map, Bucket incoming)
{
    size_t slot = map->hash(incoming.key, map->key_size) & (map->cap - 1);
    incoming.psl = 1;
    for (;;)
    {
        Bucket *cur = &map->buckets[slot];
        if (cur->psl == 0)
        {
            *cur = incoming;
            if (incoming.psl > map->max_psl)
                map->max_psl = incoming.psl;
            map->len++;
            return;
        }
        if (map->eq(cur->key, incoming.key, map->key_size))
        {
            /* update in-place: install new value, free displaced copies */
            void *old_val = cur->value;
            cur->value = incoming.value;
            if (map->allocator.free)
            {
                map->allocator.free(map->allocator.ctx, incoming.key);
                map->allocator.free(map->allocator.ctx, old_val);
            }
            return;
        }
        if (cur->psl < incoming.psl)
        {
            Bucket tmp = *cur; /* pointer swap — no allocation */
            *cur = incoming;
            if (incoming.psl > map->max_psl)
                map->max_psl = incoming.psl;
            incoming = tmp;
        }
        incoming.psl++;
        assert(incoming.psl != 0);
        slot = (slot + 1) & (map->cap - 1);
    }
}

static SeqcStatus hashmap_resize_and_rehash(HashMap *map, size_t new_cap)
{
    Bucket *old_buckets = map->buckets;
    size_t old_cap = map->cap;

    Bucket *new_buckets = map->allocator.alloc(
        map->allocator.ctx, new_cap * sizeof(Bucket), _Alignof(Bucket));
    if (!new_buckets)
        return SEQC_OOM;
    memset(new_buckets, 0, new_cap * sizeof(Bucket));

    map->buckets = new_buckets;
    map->cap = new_cap;
    map->len = 0;
    map->max_psl = 0; /* recalculated during re-insertion below */

    for (size_t i = 0; i < old_cap; i++)
    {
        if (old_buckets[i].psl != 0)
            hashmap_insert_raw(map, old_buckets[i]); /* transfers ownership */
    }
    if (map->allocator.free)
        map->allocator.free(map->allocator.ctx, old_buckets);
    return SEQC_OK;
}

SeqcStatus hashmap_set(HashMap *map, const void *key, const void *value)
{
    if (!map || !map->buckets || !key || !value)
        return SEQC_INVALID;
    if ((map->len + 1) * 4 > map->cap * 3 || map->max_psl >= HASH_PSL_THRESHOLD)
    {
        SeqcStatus rs = hashmap_resize_and_rehash(map, map->cap * 2);
        if (rs != SEQC_OK)
            return rs;
    }

    // Allocate copies once — this is the only allocation point
    void *key_copy = map->allocator.alloc(
        map->allocator.ctx, map->key_size, _Alignof(max_align_t));
    if (!key_copy)
        return SEQC_OOM;
    void *val_copy = map->allocator.alloc(
        map->allocator.ctx, map->val_size, _Alignof(max_align_t));
    if (!val_copy)
    {
        if (map->allocator.free)
            map->allocator.free(map->allocator.ctx, key_copy);
        return SEQC_OOM;
    }
    memcpy(key_copy, key, map->key_size);
    memcpy(val_copy, value, map->val_size);

    hashmap_insert_raw(map, (Bucket){key_copy, val_copy, 0});
    return SEQC_OK;
}

HashMap *hashmap_create(
    size_t key_size,
    size_t val_size,
    hash_fn hash,
    eq_fn eq,
    Allocator allocator)
{

    if (!allocator.alloc || key_size == 0 || val_size == 0 || !hash || !eq)
    {
        return NULL;
    }

    HashMap *m =
        allocator.alloc(allocator.ctx, sizeof(HashMap), _Alignof(HashMap));
    if (!m)
        return NULL;

    size_t cap = 16;

    Bucket *buckets =
        allocator.alloc(allocator.ctx, cap * sizeof(Bucket), _Alignof(Bucket));
    if (!buckets)
    {
        if (allocator.free)
            allocator.free(allocator.ctx, m);
        return NULL;
    }
    memset(buckets, 0, cap * sizeof(Bucket));
    *m = (HashMap){.buckets = buckets,
                   .cap = cap,
                   .len = 0,
                   .key_size = key_size,
                   .val_size = val_size,
                   .hash = hash,
                   .eq = eq,
                   .allocator = allocator};
    return m;
}

void hashmap_free(HashMap *map)
{
    if (!map || !map->buckets)
    {
        return; /* nothing to free */
    }
    if (map->allocator.free)
    {
        for (size_t i = 0; i < map->cap; i++)
        {
            if (map->buckets[i].psl != 0)
            {
                map->allocator.free(map->allocator.ctx, map->buckets[i].key);
                map->allocator.free(map->allocator.ctx, map->buckets[i].value);
            }
        }
        map->allocator.free(map->allocator.ctx, map->buckets);
    }
    Allocator al = map->allocator;
    if (al.free)
        al.free(al.ctx, map);
}

void hashmap_clear(HashMap *map)
{
    if (!map || !map->buckets)
        return;
    if (map->allocator.free)
    {
        for (size_t i = 0; i < map->cap; i++)
        {
            if (map->buckets[i].psl != 0)
            {
                map->allocator.free(map->allocator.ctx, map->buckets[i].key);
                map->allocator.free(map->allocator.ctx, map->buckets[i].value);
            }
        }
    }
    memset(map->buckets, 0, map->cap * sizeof(Bucket));
    map->len = 0;
    map->max_psl = 0;
}

size_t hashmap_len(const HashMap *map)
{
    return map ? map->len : 0;
}

bool hashmap_is_empty(const HashMap *map)
{
    return hashmap_len(map) == 0;
}

bool hashmap_contains(const HashMap *map, const void *key)
{
    return hashmap_get(map, key, NULL) == SEQC_OK;
}

bool hashmap_is_healthy(const HashMap *map)
{
    return map && map->max_psl <= HASH_PSL_THRESHOLD / 2;
}

HashMapStats hashmap_audit(const HashMap *map)
{
    if (!map)
        return (HashMapStats){0};
    double sum_psl = 0;
    uint8_t max_psl = 0;
    for (size_t i = 0; i < map->cap; i++)
    {
        uint8_t p = map->buckets[i].psl;
        if (p != 0)
        {
            sum_psl += p;
            if (p > max_psl)
                max_psl = p;
        }
    }
    double load_factor = map->cap > 0 ? (double)map->len / map->cap : 0.0;
    double mean_psl = map->len > 0 ? sum_psl / (double)map->len : 0.0;
    return (HashMapStats){
        .len = map->len,
        .cap = map->cap,
        .load_factor = load_factor,
        .max_psl = max_psl,
        .mean_psl = mean_psl,
        .is_healthy = mean_psl < 3.0 && max_psl <= HASH_PSL_THRESHOLD / 2,
    };
}

SeqcStatus hashmap_get(const HashMap *map, const void *key, void *out)
{
    if (!map || !map->buckets || !key)
        return SEQC_INVALID;
    size_t slot = hashmap_get_slot(map, key);

    uint8_t probe = 1;
    while (1)
    {
        uint8_t cur_psl = map->buckets[slot].psl;
        if (cur_psl == 0 || cur_psl < probe)
            return SEQC_NOT_FOUND;
        if (map->eq(map->buckets[slot].key, key, map->key_size))
        {
            if (out)
                memcpy(out, map->buckets[slot].value, map->val_size);
            return SEQC_OK;
        }
        slot = (slot + 1) & (map->cap - 1);
        probe++;
    }
}

SeqcStatus hashmap_delete(HashMap *map, const void *key)
{
    if (!map || !map->buckets || !key)
        return SEQC_INVALID;
    size_t slot = hashmap_get_slot(map, key);

    uint8_t probe = 1;
    while (1)
    {
        uint8_t cur_psl = map->buckets[slot].psl;
        if (cur_psl == 0 || cur_psl < probe)
            return SEQC_NOT_FOUND;
        if (map->eq(map->buckets[slot].key, key, map->key_size))
        {
            if (map->allocator.free)
            {
                map->allocator.free(map->allocator.ctx, map->buckets[slot].key);
                map->allocator.free(
                    map->allocator.ctx, map->buckets[slot].value);
            }
            map->buckets[slot].psl = 0;
            map->len--;

            size_t prev = slot;
            slot = (slot + 1) & (map->cap - 1);
            while (map->buckets[slot].psl > 1)
            {
                map->buckets[prev] = map->buckets[slot];
                map->buckets[prev].psl--;
                map->buckets[slot].psl = 0;
                prev = slot;
                slot = (slot + 1) & (map->cap - 1);
            }
            return SEQC_OK;
        }
        slot = (slot + 1) & (map->cap - 1);
        probe++;
    }
}

/* ---- hashmap_iter / hashmap_iter_rev ----------------------------------- */

typedef struct
{
    const HashMap *map;
    size_t slot;
} HashMapIterState;

static bool hashmap_iter_next(Iter *it, void *out)
{
    HashMapIterState *s = it->state;
    if (!s)
        return false;
    while (s->slot < s->map->cap)
    {
        Bucket *b = &s->map->buckets[s->slot++];
        if (b->psl != 0)
        {
            HashMapEntry entry = {b->key, b->value};
            memcpy(out, &entry, sizeof(HashMapEntry));
            return true;
        }
    }
    return false;
}

static bool hashmap_iter_rev_next(Iter *it, void *out)
{
    HashMapIterState *s = it->state;
    if (!s || s->slot == 0)
        return false;
    while (s->slot > 0)
    {
        Bucket *b = &s->map->buckets[--s->slot];
        if (b->psl != 0)
        {
            HashMapEntry entry = {b->key, b->value};
            memcpy(out, &entry, sizeof(HashMapEntry));
            return true;
        }
    }
    return false;
}

static void hashmap_iter_drop(Iter *it)
{
    HashMapIterState *s = it->state;
    if (s)
    {
        if (s->map->allocator.free)
        {
            s->map->allocator.free(s->map->allocator.ctx, s);
        }
    }
}

Iter hashmap_iter(const HashMap *map)
{
    if (!map || !map->buckets)
    {
        return (Iter){0}; /* invalid map */
    }
    HashMapIterState *s = map->allocator.alloc(
        map->allocator.ctx,
        sizeof(HashMapIterState),
        _Alignof(HashMapIterState));
    if (s)
        *s = (HashMapIterState){map, 0};
    return (Iter){.next = hashmap_iter_next,
                  .drop = hashmap_iter_drop,
                  .state = s,
                  .elem_size = sizeof(HashMapEntry),
                  .allocator = map->allocator};
}

Iter hashmap_iter_rev(const HashMap *map)
{
    if (!map || !map->buckets)
    {
        return (Iter){0};
    }
    HashMapIterState *s = map->allocator.alloc(
        map->allocator.ctx,
        sizeof(HashMapIterState),
        _Alignof(HashMapIterState));
    if (s)
        *s = (HashMapIterState){map, map->cap}; /* start past the last slot */
    return (Iter){.next = hashmap_iter_rev_next,
                  .drop = hashmap_iter_drop,
                  .state = s,
                  .elem_size = sizeof(HashMapEntry),
                  .allocator = map->allocator};
}

SeqcStatus hashmap_set_all(HashMap *map, Iter it)
{
    if (!map)
    {
        iter_drop(&it);
        return SEQC_INVALID;
    }
    HashMapEntry e;
    SeqcStatus st = SEQC_OK;
    while (it.next(&it, &e))
    {
        st = hashmap_set(map, e.key, e.value);
        if (st != SEQC_OK)
            break;
    }
    iter_drop(&it);
    return st;
}
