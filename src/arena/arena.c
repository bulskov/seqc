#include "arena.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#define ARENA_BLOCK_SIZE 4096

struct mem_block {
  char *buf;
  size_t pos;
  size_t cap;
  struct mem_block *next;
};

struct Arena {
  char *buf;
  struct mem_block *mem_head;
};

static size_t arena_align_to_block_size(size_t size) {
  return ((size + ARENA_BLOCK_SIZE - 1) / ARENA_BLOCK_SIZE) * ARENA_BLOCK_SIZE;
}

static struct mem_block *arena_create_block(size_t capacity, size_t offset) {
  void *mem = mmap(NULL, capacity, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (mem == MAP_FAILED) {
    return NULL;
  }
  struct mem_block *block = (struct mem_block *)((char *)mem + offset);
  block->pos = offset + sizeof(struct mem_block);
  block->buf = mem;
  block->cap = capacity;
  block->next = NULL;
  return block;
}

static void *arena_alloc_from_block(struct mem_block *block, size_t size,
                                    size_t align) {
  size_t pad = (align - (block->pos % align)) % align;
  if (block->pos + pad + size > block->cap)
    return NULL;
  void *ptr = block->buf + block->pos + pad;
  block->pos += pad + size;
  return ptr;
}

static void *arena_alloc_mem(Arena *arena, size_t size, size_t align) {
  if (size == 0)
    return NULL;
  struct mem_block *block = arena->mem_head;
  if (align == 0)
    align = 1;
  void *ptr = arena_alloc_from_block(block, size, align);
  if (ptr)
    return ptr;

  while (block->next) {
    block = block->next;
    void *ptr = arena_alloc_from_block(block, size, align);
    if (ptr)
      return ptr;
  }

  // we are out of memory in existing blocks, so we need to allocate a new one
  // block is pointing to the last block, so we can just append a new one

  size_t block_cap = arena_align_to_block_size(size + sizeof(struct mem_block));
  struct mem_block *new_block = arena_create_block(block_cap, 0);
  if (!new_block) {
    return NULL;
  }
  block->next = new_block;
  return arena_alloc_from_block(new_block, size, align);
}

Arena *arena_create(size_t capacity) {
  assert(capacity > 0);

  capacity += sizeof(Arena) + sizeof(struct mem_block);

  // align capacity to block size, so we can fit the arena struct and the first
  // block
  capacity = arena_align_to_block_size(capacity);

  struct mem_block *block = arena_create_block(capacity, sizeof(Arena));
  if (!block) {
    return NULL;
  }

  Arena *a = (Arena *)block->buf;
  a->mem_head = block;
  return a;
}

void *arena_alloc(Arena *arena, size_t size, size_t align) {
  if (size == 0)
    return NULL;

  if (align == 0)
    align = 1;

  return arena_alloc_mem(arena, size, align);
}

void arena_reset(Arena *arena) {
  struct mem_block *block = arena->mem_head;
  block->pos = sizeof(Arena) + sizeof(struct mem_block);
  while (block->next) {
    block = block->next;
    block->pos = sizeof(struct mem_block);
  }
}

// this is a recursive function that frees all blocks in the arena
// properly not the best solution if we expect a lot of blocks,
//  but it is simple and works for our use case
static void arena_free_block(struct mem_block *block) {
  if (block->next)
    arena_free_block(block->next);
  munmap(block->buf, block->cap);
}

void arena_free(Arena *a) { arena_free_block(a->mem_head); }
