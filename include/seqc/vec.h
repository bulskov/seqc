#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/allocator.h"
#include "seqc/iter.h"

typedef struct vec_t vec_t;

vec_t *vec_create(size_t elem_size, allocator_t allocator);
vec_t *vec_create_with_cap(
    size_t elem_size, size_t capacity, allocator_t allocator);
size_t vec_len(const vec_t *v);
bool vec_is_empty(const vec_t *v);
size_t vec_elem_size(const vec_t *v);
size_t vec_cap(const vec_t *v);
seqc_status_t vec_push(vec_t *v, const void *elem);
seqc_status_t vec_pop(
    vec_t *v, void *out); /* out may be NULL; SEQC_NOT_FOUND if empty */
/* Element i.  Copies into out (may be NULL to test only), so it is not affected
 * by later changes. SEQC_NOT_FOUND if i >= len, SEQC_INVALID if v is NULL. */
seqc_status_t vec_get(const vec_t *v, size_t i, void *out);
/* Pointer to element i, NULL if i >= len — valid only until the next change to
 * the vec: anything that may reallocate (vec_push, vec_insert, vec_reserve) or
 * shift elements (vec_insert, vec_remove). */
void *vec_get_ptr(const vec_t *v, size_t i);
void vec_set(vec_t *v, size_t i, const void *elem); /* overwrite element at i */
seqc_status_t vec_insert(
    vec_t *v, size_t i, const void *elem); /* shift [i..] right */
void vec_remove(vec_t *v, size_t i);       /* shift [i+1..] left */
seqc_status_t vec_reserve(
    vec_t *v, size_t capacity); /* ensure cap >= capacity */
slice_t vec_as_slice(const vec_t *v);
/* Linear search using pred(elem, ctx). Returns pointer to first match or NULL.
 * Same invalidation rules as vec_get_ptr. */
void *vec_find(const vec_t *v, pred_fn pred, void *ctx);
bool vec_any(const vec_t *v, pred_fn pred, void *ctx);
iter_t vec_iter(const vec_t *v);
iter_t vec_iter_rev(const vec_t *v);
/* Drain iter, pushing each element into v; stops and returns on OOM. */
seqc_status_t vec_extend(vec_t *v, iter_t it);
void vec_sort(vec_t *v, compare_fn cmp); /* sort in-place; no allocation */
void vec_clear(vec_t *v);                /* reset len to 0, keep buffer */
void vec_destroy(vec_t *v);
