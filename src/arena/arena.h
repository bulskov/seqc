#pragma once

#include <stddef.h>

typedef struct Arena Arena;

Arena *arena_create(size_t capacity);
void *arena_alloc(Arena *a, size_t size, size_t align);
void *arena_realloc(Arena *a, void *ptr, size_t old_size, size_t new_size,
                    size_t align);
void arena_reset(Arena *a);
void arena_free(Arena *a);
size_t arena_total_allocated(const Arena *a);
size_t arena_block_count(const Arena *a);
size_t arena_capacity(const Arena *a);
