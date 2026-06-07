#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

typedef struct set_t set_t;

set_t *set_create(size_t elem_size, hash_fn hash, eq_fn eq, allocator_t allocator);
/* SEQC_OK=added, SEQC_DUPLICATE=already present, SEQC_OOM=alloc failure */
seqc_status_t set_add(set_t *s, const void *elem);
bool set_contains(const set_t *s, const void *elem);
/* SEQC_OK=removed, SEQC_NOT_FOUND=absent */
seqc_status_t set_remove(set_t *s, const void *elem);
size_t set_len(const set_t *s);
bool set_is_empty(const set_t *s);
void set_free(set_t *s);
void set_clear(set_t *s);
iter_t set_iter(const set_t *s);     /* order unspecified */
iter_t set_iter_rev(const set_t *s); /* reverse bucket-storage order */
/* Drain iter, adding each element into s; SEQC_DUPLICATE is silently skipped.
 */
seqc_status_t set_add_all(set_t *s, iter_t it);

/* --- set_t algebra -------------------------------------------------------- */

/* dest = a ∪ b  (all elements in a or b; dest must be empty or NULL) */
seqc_status_t set_union(set_t *dest, const set_t *a, const set_t *b);
/* dest = a ∩ b  (elements present in both; dest must be empty or NULL) */
seqc_status_t set_intersection(set_t *dest, const set_t *a, const set_t *b);
/* dest = a \ b  (elements in a but not in b; dest must be empty or NULL) */
seqc_status_t set_difference(set_t *dest, const set_t *a, const set_t *b);

/* --- Health / diagnostics ----------------------------------------------- */

#define SET_PSL_THRESHOLD 128

typedef struct
{
    size_t len;
    size_t cap;
    double load_factor;
    uint8_t max_psl; /* highest probe-sequence length of any stored bucket */
    double mean_psl; /* average PSL over occupied buckets */
    bool is_healthy; /* mean_psl < 3.0 and max_psl <= SET_PSL_THRESHOLD/2 */
} set_stats_t;

/* O(1) — returns false when max_psl has exceeded SET_PSL_THRESHOLD/2. */
bool set_is_healthy(const set_t *s);

/* O(n) — full diagnostic scan; returns per-bucket statistics. */
set_stats_t set_audit(const set_t *s);
