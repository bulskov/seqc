#include "hashmap.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

size_t hashmap_fnv1a(const void *key, size_t key_size) {
  const uint8_t *data = (const uint8_t *)key;
  uint64_t hash = 14695981039346656037ULL;
  for (size_t i = 0; i < key_size; i++) {
    hash ^= (uint64_t)data[i];
    hash *= 1099511628211ULL;
  }
  return (size_t)hash;
}

int hashmap_eq_bytes(const void *a, const void *b, size_t key_size) {
  return memcmp(a, b, key_size) == 0;
}

size_t hashmap_fnv1a_str(const void *key, size_t key_size) {
  (void)key_size;
  const char *s = *(const char *const *)key;
  uint64_t hash = 14695981039346656037ULL;
  for (; *s; s++) {
    hash ^= (uint64_t)(unsigned char)*s;
    hash *= 1099511628211ULL;
  }
  return (size_t)hash;
}

int hashmap_eq_str(const void *a, const void *b, size_t key_size) {
  (void)key_size;
  return strcmp(*(const char *const *)a, *(const char *const *)b) == 0;
}

static size_t hashmap_get_slot(const HashMap *map, const void *key) {
  size_t h = map->hash(key, map->key_size);
  return h & (map->cap - 1);
}

static void hashmap_set_bucket(HashMap *map, size_t slot, Bucket *bucket) {
  Bucket *b = &map->buckets[slot];
  b->key = map->allocator.alloc(map->allocator.ctx, map->key_size,
                                _Alignof(max_align_t));
  b->value = map->allocator.alloc(map->allocator.ctx, map->val_size,
                                  _Alignof(max_align_t));
  memcpy(b->key, bucket->key, map->key_size);
  memcpy(b->value, bucket->value, map->val_size);
  b->psl = bucket->psl;
}

static void hashmap_resize_and_rehash(HashMap *map, size_t new_cap) {
  Bucket *old_buckets = map->buckets;
  size_t old_cap = map->cap;

  Bucket *new_buckets = map->allocator.alloc(
      map->allocator.ctx, new_cap * sizeof(Bucket), _Alignof(Bucket));
  memset(new_buckets, 0, new_cap * sizeof(Bucket));

  map->buckets = new_buckets;
  map->cap = new_cap;
  map->len = 0;

  for (size_t i = 0; i < old_cap; i++) {
    if (old_buckets[i].psl != 0) {
      hashmap_set(map, old_buckets[i].key, old_buckets[i].value);
      if (map->allocator.free) {
        map->allocator.free(map->allocator.ctx, old_buckets[i].key);
        map->allocator.free(map->allocator.ctx, old_buckets[i].value);
      }
    }
  }
  if (map->allocator.free) {
    map->allocator.free(map->allocator.ctx, old_buckets);
  }
}

HashMap hashmap_create(size_t key_size, size_t val_size, hash_fn hash, eq_fn eq,
                       Allocator allocator) {
  assert(key_size > 0);
  assert(val_size > 0);
  assert(hash);
  assert(eq);
  size_t cap = 16;
  Bucket *buckets =
      allocator.alloc(allocator.ctx, cap * sizeof(Bucket), _Alignof(Bucket));
  if (!buckets)
    return (HashMap){0};
  memset(buckets, 0, cap * sizeof(Bucket));
  return (HashMap){.buckets = buckets,
                   .cap = cap,
                   .len = 0,
                   .key_size = key_size,
                   .val_size = val_size,
                   .hash = hash,
                   .eq = eq,
                   .allocator = allocator};
}

void hashmap_free(HashMap *map) {
  if (map->buckets && map->allocator.free) {
    for (size_t i = 0; i < map->cap; i++) {
      if (map->buckets[i].psl != 0) {
        map->allocator.free(map->allocator.ctx, map->buckets[i].key);
        map->allocator.free(map->allocator.ctx, map->buckets[i].value);
      }
    }
    map->allocator.free(map->allocator.ctx, map->buckets);
    *map = (HashMap){0};
  }
}

int hashmap_set(HashMap *map, const void *key, const void *value) {
  if ((map->len + 1) * 4 > map->cap * 3) {
    hashmap_resize_and_rehash(map, map->cap * 2);
  }

  Bucket bucket = {.key = (void *)key, .value = (void *)value, .psl = 1};
  size_t slot = hashmap_get_slot(map, key);

  while (1) {
    if (map->buckets[slot].psl == 0) {
      hashmap_set_bucket(map, slot, &bucket);
      map->len++;
      return 1;
    }
    if (map->eq(map->buckets[slot].key, bucket.key, map->key_size)) {
      memcpy(map->buckets[slot].value, bucket.value, map->val_size);
      return 1;
    }
    if (map->buckets[slot].psl < bucket.psl) {
      /* Robin Hood: steal the slot and reinsert the displaced bucket */
      Bucket temp = map->buckets[slot];
      hashmap_set_bucket(map, slot, &bucket);
      bucket = temp;
    }
    bucket.psl++;
    slot = (slot + 1) & (map->cap - 1);
  }
  return 0;
}

void *hashmap_get(const HashMap *map, const void *key) {
  size_t slot = hashmap_get_slot(map, key);

  while (1) { /* probe until we find an empty slot or a match */
    if (map->buckets[slot].psl == 0) {
      return NULL;
    }
    if (map->eq(map->buckets[slot].key, key, map->key_size)) {
      return map->buckets[slot].value;
    }
    slot = (slot + 1) & (map->cap - 1);
  }
}

int hashmap_delete(HashMap *map, const void *key) {
  size_t slot = hashmap_get_slot(map, key);

  while (1) {
    if (map->buckets[slot].psl == 0) {
      return 0;
    }
    if (map->eq(map->buckets[slot].key, key, map->key_size)) {
      map->buckets[slot].psl = 0; /* mark as deleted */
      map->len--;

      size_t prev = slot;
      slot = (slot + 1) & (map->cap - 1);
      while (map->buckets[slot].psl > 1) {
        map->buckets[prev] = map->buckets[slot];
        map->buckets[prev].psl--;
        map->buckets[slot].psl = 0;
        prev = slot;
        slot = (slot + 1) & (map->cap - 1);
      }

      return 1;
    }
    slot = (slot + 1) & (map->cap - 1);
  }
}
