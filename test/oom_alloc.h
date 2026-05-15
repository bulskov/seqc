#pragma once

/*
 * Test-only allocators for OOM path coverage.
 *
 *   null_allocator()   — every alloc/realloc returns NULL immediately
 *   oom_after(n)       — returns NULL on the (n+1)-th alloc/realloc call
 *
 * Usage:
 *   OomCtx ctx = {0};
 *   allocator_t al = oom_after_allocator(2, &ctx);
 *   // first 2 alloc calls succeed (via malloc), 3rd returns NULL
 */

#include <stddef.h>
#include <stdlib.h>

#include "arena/allocator.h"

/* ---- sys_allocator (malloc/realloc/free) -------------------------------- */

static void *sys_alloc_fn(void *ctx, size_t size, size_t align)
{
    (void)ctx; (void)align;
    return malloc(size);
}

static void *sys_realloc_fn(void *ctx, void *ptr, size_t old_size,
                            size_t new_size, size_t align)
{
    (void)ctx; (void)old_size; (void)align;
    return realloc(ptr, new_size);
}

static void sys_free_fn(void *ctx, void *ptr, size_t size)
{
    (void)ctx; (void)size;
    free(ptr);
}

static const allocator_vtable_t sys_vtable_g = {
    sys_alloc_fn, sys_realloc_fn, sys_free_fn};

static inline allocator_t sys_allocator(void)
{
    return (allocator_t){.vt = &sys_vtable_g, .ctx = NULL};
}

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

static void null_free(void *ctx, void *ptr, size_t size)
{
    (void)ctx; (void)ptr; (void)size;
}

static const allocator_vtable_t null_allocator_vtable = {
    .alloc   = null_alloc,
    .realloc = null_realloc,
    .free    = null_free,
};

static allocator_t null_allocator(void)
{
    return (allocator_t){ .vt = &null_allocator_vtable, .ctx = NULL };
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

static void oom_free(void *ctx, void *ptr, size_t size)
{
    (void)ctx; (void)size;
    free(ptr);
}

static const allocator_vtable_t oom_allocator_vtable = {
    .alloc   = oom_alloc,
    .realloc = oom_realloc,
    .free    = oom_free,
};

/* n = number of successful alloc/realloc calls before the first failure */
static allocator_t oom_after_allocator(size_t n, OomCtx *ctx)
{
    ctx->remaining = n;
    return (allocator_t){ .vt = &oom_allocator_vtable, .ctx = ctx };
}
