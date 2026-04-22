#include "hashmap.h"
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
    hash_fn hash;
    eq_fn eq;
    Allocator allocator;
};

size_t hashmap_fnv1a(const void *key, size_t key_size)
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

bool hashmap_eq_bytes(const void *a, const void *b, size_t key_size)
{
    if (key_size == 0)
        return true; /* treat all zero-size keys as equal */
    if (a == NULL || b == NULL)
        return false; /* treat null pointers as unequal to any key, including
                       * each other
                       */
    return memcmp(a, b, key_size) == 0;
}

size_t hashmap_fnv1a_str(const void *key, size_t key_size)
{
    if (key_size != sizeof(char *) || key == NULL)
    {
        return 0; /* invalid key size for string map */
    }
    const char *s = *(const char *const *)key;
    uint64_t hash = 14695981039346656037ULL;
    for (; *s; s++)
    {
        hash ^= (uint64_t)(unsigned char)*s;
        hash *= 1099511628211ULL;
    }
    return (size_t)hash;
}

bool hashmap_eq_str(const void *a, const void *b, size_t key_size)
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

static void hashmap_set_bucket(HashMap *map, size_t slot, Bucket *bucket)
{
    Bucket *b = &map->buckets[slot];
    b->key = map->allocator.alloc(
        map->allocator.ctx, map->key_size, _Alignof(max_align_t));
    b->value = map->allocator.alloc(
        map->allocator.ctx, map->val_size, _Alignof(max_align_t));
    memcpy(b->key, bucket->key, map->key_size);
    memcpy(b->value, bucket->value, map->val_size);
    b->psl = bucket->psl;
}

static void hashmap_resize_and_rehash(HashMap *map, size_t new_cap)
{
    Bucket *old_buckets = map->buckets;
    size_t old_cap = map->cap;

    Bucket *new_buckets = map->allocator.alloc(
        map->allocator.ctx, new_cap * sizeof(Bucket), _Alignof(Bucket));
    memset(new_buckets, 0, new_cap * sizeof(Bucket));

    map->buckets = new_buckets;
    map->cap = new_cap;
    map->len = 0;

    for (size_t i = 0; i < old_cap; i++)
    {
        if (old_buckets[i].psl != 0)
        {
            hashmap_set(map, old_buckets[i].key, old_buckets[i].value);
            if (map->allocator.free)
            {
                map->allocator.free(map->allocator.ctx, old_buckets[i].key);
                map->allocator.free(map->allocator.ctx, old_buckets[i].value);
            }
        }
    }
    if (map->allocator.free)
    {
        map->allocator.free(map->allocator.ctx, old_buckets);
    }
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
        return NULL;
    memset(buckets, 0, cap * sizeof(Bucket));
    *m = (HashMap){
        .buckets = buckets,
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
}

size_t hashmap_len(const HashMap *map)
{
    return map ? map->len : 0;
}

bool hashmap_contains(const HashMap *map, const void *key)
{
    return hashmap_get(map, key) != NULL;
}

bool hashmap_set(HashMap *map, const void *key, const void *value)
{
    if (!map || !map->buckets || !key || !value)
    {
        return false; /* invalid map or key/value */
    }
    if ((map->len + 1) * 4 > map->cap * 3)
    {
        hashmap_resize_and_rehash(map, map->cap * 2);
    }

    Bucket bucket = {.key = (void *)key, .value = (void *)value, .psl = 1};
    size_t slot = hashmap_get_slot(map, key);

    while (1)
    {
        if (map->buckets[slot].psl == 0)
        {
            hashmap_set_bucket(map, slot, &bucket);
            map->len++;
            return true;
        }
        if (map->eq(map->buckets[slot].key, bucket.key, map->key_size))
        {
            memcpy(map->buckets[slot].value, bucket.value, map->val_size);
            return true;
        }
        if (map->buckets[slot].psl < bucket.psl)
        {
            /* Robin Hood: steal the slot and reinsert the displaced bucket */
            Bucket temp = map->buckets[slot];
            hashmap_set_bucket(map, slot, &bucket);
            bucket = temp;
        }
        bucket.psl++;
        slot = (slot + 1) & (map->cap - 1);
    }
    return false;
}

void *hashmap_get(const HashMap *map, const void *key)
{
    if (!map || !map->buckets || !key)
    {
        return NULL; /* invalid map or key */
    }
    size_t slot = hashmap_get_slot(map, key);

    while (1)
    { /* probe until we find an empty slot or a match */
        if (map->buckets[slot].psl == 0)
        {
            return NULL;
        }
        if (map->eq(map->buckets[slot].key, key, map->key_size))
        {
            return map->buckets[slot].value;
        }
        slot = (slot + 1) & (map->cap - 1);
    }
}

bool hashmap_delete(HashMap *map, const void *key)
{
    if (!map || !map->buckets || !key)
    {
        return false; /* invalid map or key */
    }
    size_t slot = hashmap_get_slot(map, key);

    while (1)
    {
        if (map->buckets[slot].psl == 0)
        {
            return false;
        }
        if (map->eq(map->buckets[slot].key, key, map->key_size))
        {
            map->buckets[slot].psl = 0; /* mark as deleted */
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

            return true;
        }
        slot = (slot + 1) & (map->cap - 1);
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
    return (Iter){
        .next = hashmap_iter_next,
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
    return (Iter){
        .next = hashmap_iter_rev_next,
        .drop = hashmap_iter_drop,
        .state = s,
        .elem_size = sizeof(HashMapEntry),
        .allocator = map->allocator};
}
