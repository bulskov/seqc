#pragma once

#include <stddef.h>

typedef struct Arena Arena;

Arena *arena_create(size_t capacity);
void *arena_alloc(Arena *a, size_t size, size_t align);
void arena_reset(Arena *a);
void arena_free(Arena *a);
