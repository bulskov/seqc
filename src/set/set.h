#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h"
#include "iter/iter.h"

/* Function pointer types for hashing and equality — same signatures as the
 * hashmap equivalents so hashmap_fnv1a / hashmap_eq_bytes etc. are usable
 * directly without casts. */
typedef size_t (*set_hash_fn)(const void *key, size_t key_size);
typedef int (*set_eq_fn)(const void *a, const void *b, size_t key_size);

typedef struct {
  void *key;
  uint8_t psl; /* probe-sequence length; 0 = empty */
} SetBucket;

typedef struct {
  SetBucket *buckets;
  size_t cap; /* always a power of 2 */
  size_t len;
  size_t elem_size;
  set_hash_fn hash;
  set_eq_fn eq;
  Allocator allocator;
} Set;

Set set_create(size_t elem_size, set_hash_fn hash, set_eq_fn eq,
               Allocator allocator);
bool set_add(Set *s, const void *elem); /* true=added false=already present */
bool set_contains(const Set *s, const void *elem);
bool set_remove(Set *s, const void *elem); /* true=removed false=not found */
size_t set_len(const Set *s);
void set_free(Set *s);
Iter set_iter(const Set *s); /* order unspecified */
