#pragma once

#include <stddef.h>

typedef void *(*alloc_fn)(void *ctx, size_t size, size_t align);
typedef void *(*realloc_fn)(void *ctx, void *ptr, size_t old_size,
                            size_t new_size, size_t align);
typedef void (*free_fn)(void *ctx, void *ptr);

typedef struct Allocator {
  alloc_fn alloc;
  realloc_fn realloc;
  free_fn free; /* no-op for arena */
  void *ctx;
} Allocator;

typedef struct Arena Arena;

Allocator arena_allocator(Arena *arena);

typedef struct {
  Arena *arena;
  void *saved_tail; /* opaque MemBlock * */
  size_t saved_pos;
} Scratch;

Scratch arena_scratch_push(Arena *arena);
void arena_scratch_pop(Scratch *scratch);
Allocator scratch_allocator(Scratch *scratch);

Arena *arena_create(size_t capacity);
void *arena_alloc(Arena *a, size_t size, size_t align);
void *arena_realloc(Arena *a, void *ptr, size_t old_size, size_t new_size,
                    size_t align);
void arena_reset(Arena *a);
void arena_free(Arena *a);
size_t arena_total_allocated(const Arena *a);
size_t arena_block_count(const Arena *a);
size_t arena_capacity(const Arena *a);
