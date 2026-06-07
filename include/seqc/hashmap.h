#pragma once

#include "arena/allocator.h"
#include "seqc/iter.h"
#include <stddef.h>
#include <stdint.h>

typedef struct hashmap_t hashmap_t;

/* Pointer-pair yielded by hashmap_iter — see map_entry_t in iter/iter.h.
 * Do not modify the map while iterating. */
typedef map_entry_t hashmap_entry_t;

hashmap_t *hashmap_create(
    size_t key_size,
    size_t val_size,
    hash_fn hash,
    eq_fn eq,
    allocator_t allocator);
void hashmap_free(hashmap_t *map);
void hashmap_clear(hashmap_t *map);
size_t hashmap_len(const hashmap_t *map);
bool hashmap_is_empty(const hashmap_t *map);
/* Drain iter of map_entry_t, inserting each key-value pair into map. */
seqc_status_t hashmap_set_all(hashmap_t *map, iter_t it);
bool hashmap_contains(const hashmap_t *map, const void *key);
seqc_status_t hashmap_set(hashmap_t *map, const void *key, const void *value);
/* Copies the value for key into out (may be NULL to test for presence only).
 * Returns SEQC_OK if found, SEQC_NOT_FOUND otherwise. */
seqc_status_t hashmap_get(const hashmap_t *map, const void *key, void *out);
seqc_status_t hashmap_delete(hashmap_t *map, const void *key);
/* Yields hashmap_entry_t pairs in unspecified order.
 * Do not modify the map while iterating. */
iter_t hashmap_iter(const hashmap_t *map);
iter_t hashmap_iter_rev(const hashmap_t *map);

/* --- Health / diagnostics ----------------------------------------------- */

typedef struct
{
    size_t len;
    size_t cap;
    double load_factor;
    uint8_t max_psl; /* highest probe-sequence length of any stored bucket */
    double mean_psl; /* average PSL over occupied buckets */
    bool is_healthy; /* mean_psl < 3.0 and max_psl <= HASH_PSL_THRESHOLD/2 */
} hashmap_stats_t;

/* O(1) — returns false when max_psl has exceeded HASH_PSL_THRESHOLD/2,
 * indicating a poor hash function. */
bool hashmap_is_healthy(const hashmap_t *map);

/* O(n) — full diagnostic scan; returns per-bucket statistics. */
hashmap_stats_t hashmap_audit(const hashmap_t *map);
