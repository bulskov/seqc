#pragma once

/*
 * Test-only allocators for OOM path coverage.
 *
 *   null_allocator()   — every alloc/realloc returns NULL immediately
 *   oom_after(n)       — returns NULL on the (n+1)-th alloc/realloc call
 *
 * Usage:
 *   oom_ctx_t ctx = {0};
 *   allocator_t al = oom_after_allocator(2, &ctx);
 *   // first 2 alloc calls succeed (via malloc), 3rd returns NULL
 */

#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "arena/allocator.h"
#ifdef __cplusplus
}
#endif

/* ---- sys_allocator (malloc/realloc/free) -------------------------------- */

static void *sys_alloc_fn(void *ctx, size_t size, size_t align)
{
    (void)ctx;
    (void)align;
    return malloc(size);
}

static void *sys_realloc_fn(
    void *ctx, void *ptr, size_t old_size, size_t new_size, size_t align)
{
    (void)ctx;
    (void)old_size;
    (void)align;
    return realloc(ptr, new_size);
}

static void sys_free_fn(void *ctx, void *ptr, size_t size)
{
    (void)ctx;
    (void)size;
    free(ptr);
}

static const allocator_vtable_t sys_vtable_g = {
    sys_alloc_fn, sys_realloc_fn, sys_free_fn};

static inline allocator_t sys_allocator(void)
{
    allocator_t a = {&sys_vtable_g, NULL};
    return a;
}

/* ---- null allocator (always fails) ------------------------------------- */

static void *null_alloc(void *ctx, size_t size, size_t align)
{
    (void)ctx;
    (void)size;
    (void)align;
    return NULL;
}

static void *null_realloc(
    void *ctx, void *ptr, size_t old_size, size_t new_size, size_t align)
{
    (void)ctx;
    (void)ptr;
    (void)old_size;
    (void)new_size;
    (void)align;
    return NULL;
}

static void null_free(void *ctx, void *ptr, size_t size)
{
    (void)ctx;
    (void)ptr;
    (void)size;
}

static const allocator_vtable_t null_allocator_vtable = {
    null_alloc, null_realloc, null_free};

[[maybe_unused]] static allocator_t null_allocator(void)
{
    allocator_t a = {&null_allocator_vtable, NULL};
    return a;
}

/* ---- counting allocator (fails after n successes) ---------------------- */

typedef struct
{
    size_t remaining; /* calls left before failure */
} oom_ctx_t;

static void *oom_alloc(void *ctx, size_t size, size_t align)
{
    oom_ctx_t *c = (oom_ctx_t *)ctx;
    if (c->remaining == 0)
        return NULL;
    c->remaining--;
    (void)align;
    return malloc(size);
}

static void *oom_realloc(
    void *ctx, void *ptr, size_t old_size, size_t new_size, size_t align)
{
    oom_ctx_t *c = (oom_ctx_t *)ctx;
    if (c->remaining == 0)
        return NULL;
    c->remaining--;
    (void)old_size;
    (void)align;
    return realloc(ptr, new_size);
}

static void oom_free(void *ctx, void *ptr, size_t size)
{
    (void)ctx;
    (void)size;
    free(ptr);
}

static const allocator_vtable_t oom_allocator_vtable = {
    oom_alloc, oom_realloc, oom_free};

/* n = number of successful alloc/realloc calls before the first failure */
[[maybe_unused]] static allocator_t oom_after_allocator(size_t n, oom_ctx_t *ctx)
{
    ctx->remaining = n;
    allocator_t a = {&oom_allocator_vtable, ctx};
    return a;
}
