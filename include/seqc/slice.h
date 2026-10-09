#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "seqc/status.h"

typedef struct
{
    void *ptr;
    size_t len;
    size_t elem_size;
} slice_t;

/* Element i.  Copies into out (may be NULL to test only).  SEQC_NOT_FOUND
 * if i >= len. */
seqc_status_t slice_get(slice_t s, size_t i, void *out);
/* Pointer to element i, NULL if i >= len — valid as long as the memory the
 * slice views. */
void *slice_get_ptr(slice_t s, size_t i);

/* Linear search. pred(elem, ctx) must return true to match.
 * Returns pointer to the first matching element, or NULL. */
void *slice_find(
    slice_t s, bool (*pred)(const void *elem, void *ctx), void *ctx);
bool slice_any(slice_t s, bool (*pred)(const void *elem, void *ctx), void *ctx);
