#pragma once

/*
 * Test-only allocators for OOM path coverage.
 *
 *   null_allocator()   — every alloc/realloc returns NULL immediately
 *   oom_after(n)       — returns NULL on the (n+1)-th alloc/realloc call
 *
 * Usage:
 *   OomCtx ctx = {0};
 *   Allocator al = oom_after_allocator(2, &ctx);
 *   // first 2 alloc calls succeed (via malloc), 3rd returns NULL
 */

#include <stddef.h>
#include <stdlib.h>

#include "seqc/arena.h"

/* ---- null allocator (always fails) ------------------------------------- */

static void *null_alloc(void *ctx, size_t size, size_t align)
{
    (void)ctx; (void)size; (void)align;
    return NULL;
}

static void *null_realloc(void *ctx, void *ptr, size_t old_size,
                          size_t new_size, size_t align)
{
    (void)ctx; (void)ptr; (void)old_size; (void)new_size; (void)align;
    return NULL;
}

static Allocator null_allocator(void)
{
    return (Allocator){
        .alloc   = null_alloc,
        .realloc = null_realloc,
        .free    = NULL,
        .ctx     = NULL,
    };
}

/* ---- counting allocator (fails after n successes) ---------------------- */

typedef struct
{
    size_t remaining; /* calls left before failure */
} OomCtx;

static void *oom_alloc(void *ctx, size_t size, size_t align)
{
    OomCtx *c = (OomCtx *)ctx;
    if (c->remaining == 0)
        return NULL;
    c->remaining--;
    (void)align;
    return malloc(size);
}

static void *oom_realloc(void *ctx, void *ptr, size_t old_size,
                         size_t new_size, size_t align)
{
    OomCtx *c = (OomCtx *)ctx;
    if (c->remaining == 0)
        return NULL;
    c->remaining--;
    (void)old_size; (void)align;
    return realloc(ptr, new_size);
}

static void oom_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

/* n = number of successful alloc/realloc calls before the first failure */
static Allocator oom_after_allocator(size_t n, OomCtx *ctx)
{
    ctx->remaining = n;
    return (Allocator){
        .alloc   = oom_alloc,
        .realloc = oom_realloc,
        .free    = oom_free,
        .ctx     = ctx,
    };
}
