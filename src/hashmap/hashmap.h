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
/* Drain iter of MapEntry, inserting each key-value pair into map. */
SeqcStatus hashmap_set_all(HashMap *map, Iter it);
bool hashmap_contains(const HashMap *map, const void *key);
SeqcStatus hashmap_set(HashMap *map, const void *key, const void *value);
/* Copies the value for key into out (may be NULL to test for presence only).
 * Returns SEQC_OK if found, SEQC_NOT_FOUND otherwise. */
SeqcStatus hashmap_get(const HashMap *map, const void *key, void *out);
SeqcStatus hashmap_delete(HashMap *map, const void *key);
/* Yields HashMapEntry pairs in unspecified order.
 * Do not modify the map while iterating. */
Iter hashmap_iter(const HashMap *map);
Iter hashmap_iter_rev(const HashMap *map);

/* --- Health / diagnostics ----------------------------------------------- */

/* PSL above this triggers an automatic resize to protect against uint8_t
 * overflow (max storable PSL is 255). Indicates a degenerate hash function. */
#define HASHMAP_PSL_THRESHOLD 128

typedef struct
{
    size_t len;
    size_t cap;
    double load_factor;
    uint8_t max_psl;  /* highest probe-sequence length of any stored bucket */
    double mean_psl;  /* average PSL over occupied buckets */
    bool is_healthy;  /* mean_psl < 3.0 and max_psl <= HASHMAP_PSL_THRESHOLD/2 */
} HashMapStats;

/* O(1) — returns false when max_psl has exceeded HASHMAP_PSL_THRESHOLD/2,
 * indicating a poor hash function. */
bool hashmap_is_healthy(const HashMap *map);

/* O(n) — full diagnostic scan; returns per-bucket statistics. */
HashMapStats hashmap_audit(const HashMap *map);
