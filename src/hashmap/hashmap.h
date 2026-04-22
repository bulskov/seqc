#pragma once

#include "arena/arena.h"
#include "iter/iter.h"
#include <stddef.h>
#include <stdint.h>

typedef struct HashMap HashMap;

/* Default hash and equality functions suitable for any fixed-size key type */
size_t hashmap_fnv1a(const void *key, size_t key_size);
bool hashmap_eq_bytes(const void *a, const void *b, size_t key_size);

/* String variants: key is a (char *), key_size is sizeof(char *) */
size_t hashmap_fnv1a_str(const void *key, size_t key_size);
bool hashmap_eq_str(const void *a, const void *b, size_t key_size);

/* Pointer-pair yielded by hashmap_iter — see MapEntry in iter/iter.h.
 * Do not modify the map while iterating. */
typedef MapEntry HashMapEntry;

HashMap *hashmap_create(
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
/* Copies the value for key into out (may be NULL to test for presence only).
 * Returns true if the key was found, false otherwise. */
bool hashmap_get(const HashMap *map, const void *key, void *out);
bool hashmap_delete(HashMap *map, const void *key);
/* Yields HashMapEntry pairs in unspecified order.
 * Do not modify the map while iterating. */
Iter hashmap_iter(const HashMap *map);
Iter hashmap_iter_rev(const HashMap *map);
