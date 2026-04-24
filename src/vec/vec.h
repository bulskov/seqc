#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/arena.h"
#include "iter/iter.h"

typedef struct Vec Vec;

Vec *vec_create(size_t elem_size, Allocator allocator);
Vec *vec_create_size(size_t elem_size, size_t capacity, Allocator allocator);
size_t vec_len(const Vec *v);
size_t vec_elem_size(const Vec *v);
size_t vec_cap(const Vec *v);
SeqcStatus vec_push(Vec *v, const void *elem);
SeqcStatus vec_pop(
    Vec *v, void *out); /* out may be NULL; SEQC_NOT_FOUND if empty */
/* Returns pointer into the backing buffer at index i.
 * Invalidated by any call that may reallocate (vec_push, vec_insert,
 * vec_reserve) or shift elements (vec_insert, vec_remove). */
void *vec_get(const Vec *v, size_t i);
/* Safe copy-out variant: copies element at i into out.
 * Returns SEQC_OK, SEQC_NOT_FOUND (out of bounds), or SEQC_INVALID (NULL out).
 */
SeqcStatus vec_get_copy(const Vec *v, size_t i, void *out);
void vec_set(Vec *v, size_t i, const void *elem); /* overwrite element at i */
SeqcStatus vec_insert(
    Vec *v, size_t i, const void *elem);         /* shift [i..] right */
void vec_remove(Vec *v, size_t i);               /* shift [i+1..] left */
SeqcStatus vec_reserve(Vec *v, size_t capacity); /* ensure cap >= capacity */
Slice vec_as_slice(const Vec *v);
/* Linear search using pred(elem, ctx). Returns pointer to first match or NULL.
 * Same invalidation rules as vec_get. */
void *vec_find(const Vec *v, pred_fn pred, void *ctx);
bool vec_contains(const Vec *v, pred_fn pred, void *ctx);
Iter vec_iter(const Vec *v);
Iter vec_iter_rev(const Vec *v);
/* Drain iter, pushing each element into v; stops and returns on OOM. */
SeqcStatus vec_extend(Vec *v, Iter it);
void vec_clear(Vec *v); /* reset len to 0, keep buffer */
void vec_free(Vec *v);
