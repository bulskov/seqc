#pragma once

#include "arena/arena.h"
#include "iter/iter.h"
#include <stddef.h>
#include <stdint.h>

typedef size_t (*hash_fn)(const void *key, size_t key_size);
typedef bool (*eq_fn)(const void *a, const void *b, size_t key_size);

typedef struct
{
    void *key;
    void *value;
    uint8_t psl; /* 0 = empty */
} Bucket;

typedef struct
{
    Bucket *buckets;
    size_t cap; /* always power of 2 */
    size_t len;
    size_t key_size;
    size_t val_size;
    hash_fn hash;
    eq_fn eq;
    Allocator allocator;
} HashMap;

/* Default hash and equality functions suitable for any fixed-size key type */
size_t hashmap_fnv1a(const void *key, size_t key_size);
bool hashmap_eq_bytes(const void *a, const void *b, size_t key_size);

/* String variants: key is a (char *), key_size is sizeof(char *) */
size_t hashmap_fnv1a_str(const void *key, size_t key_size);
bool hashmap_eq_str(const void *a, const void *b, size_t key_size);

typedef struct
{
    void *key;
    void *value;
} HashMapEntry;

HashMap hashmap_create(
    size_t key_size,
    size_t val_size,
    hash_fn hash,
    eq_fn eq,
    Allocator allocator);
void hashmap_free(HashMap *map);
void hashmap_clear(HashMap *map);
size_t hashmap_len(const HashMap *map);
bool hashmap_contains(const HashMap *map, const void *key);
bool hashmap_set(HashMap *map, const void *key, const void *value);
void *hashmap_get(const HashMap *map, const void *key);
bool hashmap_delete(HashMap *map, const void *key);
Iter hashmap_iter(const HashMap *map);
Iter hashmap_iter_rev(const HashMap *map);
