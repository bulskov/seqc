#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h"
#include "iter/iter.h"

typedef struct Set Set;

Set *set_create(
    size_t elem_size, hash_fn hash, eq_fn eq, Allocator allocator);
bool set_add(Set *s, const void *elem); /* true=added false=already present */
bool set_contains(const Set *s, const void *elem);
bool set_remove(Set *s, const void *elem); /* true=removed false=not found */
size_t set_len(const Set *s);
void set_free(Set *s);
void set_clear(Set *s);
Iter set_iter(const Set *s);     /* order unspecified */
Iter set_iter_rev(const Set *s); /* reverse bucket-storage order */

/* --- Health / diagnostics ----------------------------------------------- */

#define SET_PSL_THRESHOLD 128

typedef struct
{
    size_t len;
    size_t cap;
    double load_factor;
    uint8_t max_psl;  /* highest probe-sequence length of any stored bucket */
    double mean_psl;  /* average PSL over occupied buckets */
    bool is_healthy;  /* mean_psl < 3.0 and max_psl <= SET_PSL_THRESHOLD/2 */
} SetStats;

/* O(1) — returns false when max_psl has exceeded SET_PSL_THRESHOLD/2. */
bool set_is_healthy(const Set *s);

/* O(n) — full diagnostic scan; returns per-bucket statistics. */
SetStats set_audit(const Set *s);
