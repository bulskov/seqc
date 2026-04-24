#include "seqc/arena.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ---- platform memory: mmap on POSIX, VirtualAlloc on Windows ----------- */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

static void *platform_mem_alloc(size_t size)
{
    void *p = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return p; /* NULL on failure, same sentinel as mmap failure */
}

static void platform_mem_free(void *ptr, size_t size)
{
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
}

#else /* POSIX */
#  include <sys/mman.h>
#  ifndef MAP_ANONYMOUS          /* BSDs expose MAP_ANON, not MAP_ANONYMOUS */
#    define MAP_ANONYMOUS MAP_ANON
#  endif

static void *platform_mem_alloc(size_t size)
{
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

static void platform_mem_free(void *ptr, size_t size)
{
    munmap(ptr, size);
}
#endif

#define ARENA_BLOCK_SIZE 4096

typedef struct MemBlock
{
    char *buf;
    size_t pos;
    size_t cap;
    struct MemBlock *next;
} MemBlock;

struct Arena
{
    MemBlock *mem_head;
    MemBlock *mem_tail;
};

static size_t arena_align_to_block_size(size_t size)
{
    return ((size + ARENA_BLOCK_SIZE - 1) / ARENA_BLOCK_SIZE)
           * ARENA_BLOCK_SIZE;
}

static MemBlock *arena_create_block(size_t capacity, size_t offset)
{
    void *mem = platform_mem_alloc(capacity);
    if (!mem)
        return NULL;
    MemBlock *block = (MemBlock *)((char *)mem + offset);
    block->pos = offset + sizeof(MemBlock);
    block->buf = mem;
    block->cap = capacity;
    block->next = NULL;
    return block;
}

static void *arena_alloc_from_block(MemBlock *block, size_t size, size_t align)
{
    size_t pad = (align - (block->pos % align)) % align;
    if (block->pos + pad + size > block->cap)
        return NULL;
    void *ptr = (void *)((char *)block->buf + (block->pos + pad));
    block->pos += pad + size;
    return ptr;
}

static void *arena_alloc_mem(Arena *arena, size_t size, size_t align)
{
    if (size == 0)
        return NULL;
    MemBlock *block = arena->mem_tail;
    if (align == 0)
        align = 1;
    void *ptr = arena_alloc_from_block(block, size, align);
    if (ptr)
        return ptr;

    // we are out of memory in existing blocks, so we need to allocate a new one
    // block is pointing to the last block, so we can just append a new one

    size_t block_cap = arena_align_to_block_size(size + sizeof(MemBlock));
    MemBlock *new_block = arena_create_block(block_cap, 0);
    if (!new_block)
    {
        return NULL;
    }
    block->next = new_block;
    arena->mem_tail = new_block;
    return arena_alloc_from_block(new_block, size, align);
}

Arena *arena_create(size_t capacity)
{
    assert(capacity > 0);

    capacity += sizeof(Arena) + sizeof(MemBlock);

    // align capacity to block size, so we can fit the arena struct and the
    // first block
    capacity = arena_align_to_block_size(capacity);

    MemBlock *block = arena_create_block(capacity, sizeof(Arena));
    if (!block)
    {
        return NULL;
    }

    Arena *a = (Arena *)block->buf;
    a->mem_head = block;
    a->mem_tail = block;
    return a;
}

void *arena_alloc(Arena *arena, size_t size, size_t align)
{
    if (size == 0)
        return NULL;

    if (align == 0)
        align = 1;

    return arena_alloc_mem(arena, size, align);
}

static MemBlock *arena_find_block(Arena *arena, void *ptr)
{
    MemBlock *block = arena->mem_head;
    while (block)
    {
        if ((char *)ptr >= (char *)block->buf
            && (char *)ptr < (char *)block->buf + block->cap)
        {
            return block;
        }
        block = block->next;
    }
    return NULL;
}

void *arena_realloc(
    Arena *arena, void *ptr, size_t old_size, size_t new_size, size_t align)
{
    if (new_size == 0)
    {
        return NULL;
    }
    if (ptr == NULL)
    {
        return arena_alloc(arena, new_size, align);
    }

    MemBlock *block = arena_find_block(arena, ptr);
    if (!block)
    {
        return NULL; // ptr not allocated by this arena
    }

    if (new_size <= old_size)
    {
        return ptr;
    }

    if (block->cap - block->pos >= new_size - old_size
        && (char *)ptr + old_size == (char *)block->buf + block->pos)
    {
        // we have enough space in the current block to expand the allocation
        // and the current allocation is at the end of the used space, so we can
        // just expand it
        block->pos += new_size - old_size;
        return ptr;
    }

    void *new_ptr = arena_alloc(arena, new_size, align);
    if (!new_ptr)
    {
        return NULL;
    }
    memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
    return new_ptr;
}

void arena_reset(Arena *arena)
{
    MemBlock *block = arena->mem_head;
    block->pos = sizeof(Arena) + sizeof(MemBlock);
    while (block->next)
    {
        block = block->next;
        block->pos = sizeof(MemBlock);
    }
}

void arena_reset_hard(Arena *arena)
{
    /* Free all overflow blocks, keeping only the first (head) block */
    MemBlock *head = arena->mem_head;
    MemBlock *block = head->next;
    while (block)
    {
        MemBlock *next = block->next;
        platform_mem_free(block->buf, block->cap);
        block = next;
    }
    head->next = NULL;
    head->pos = sizeof(Arena) + sizeof(MemBlock);
    arena->mem_tail = head;
}

void arena_free(Arena *a)
{
    MemBlock *block = a->mem_head;
    while (block)
    {
        MemBlock *next = block->next;
        platform_mem_free(block->buf, block->cap);
        block = next;
    }
}

size_t arena_total_allocated(const Arena *a)
{
    size_t total = 0;
    MemBlock *block = a->mem_head;
    while (block)
    {
        total += block->pos;
        block = block->next;
    }
    return total;
}

size_t arena_block_count(const Arena *a)
{
    size_t count = 0;
    MemBlock *block = a->mem_head;
    while (block)
    {
        count++;
        block = block->next;
    }
    return count;
}

size_t arena_capacity(const Arena *a)
{
    size_t capacity = 0;
    MemBlock *block = a->mem_head;
    while (block)
    {
        capacity += block->cap;
        block = block->next;
    }
    return capacity;
}

Allocator arena_allocator(Arena *arena)
{
    assert(arena);
    return (Allocator){.alloc = (alloc_fn)arena_alloc,
                       .realloc = (realloc_fn)arena_realloc,
                       .free = (free_fn)NULL,
                       .ctx = arena};
}

Scratch arena_scratch_push(Arena *arena)
{
    MemBlock *tail = arena->mem_tail;
    return (Scratch){
        .arena = arena, .saved_tail = tail, .saved_pos = tail->pos};
}

void arena_scratch_pop(Scratch *scratch)
{
    Arena *a = scratch->arena;
    MemBlock *saved = (MemBlock *)scratch->saved_tail;
    /* free any blocks appended after the saved tail */
    MemBlock *block = saved->next;
    while (block)
    {
        MemBlock *next = block->next;
        platform_mem_free(block->buf, block->cap);
        block = next;
    }
    saved->next = NULL;
    saved->pos = scratch->saved_pos;
    a->mem_tail = saved;
}

Allocator scratch_allocator(Scratch *scratch)
{
    return arena_allocator(scratch->arena);
}

/* ---- sys_allocator ----------------------------------------------------- */

static void *sys_alloc_fn(void *ctx, size_t size, size_t align)
{
    (void)ctx;
    (void)align; /* malloc is aligned to _Alignof(max_align_t) */
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

static void sys_free_fn(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

Allocator sys_allocator(void)
{
    return (Allocator){.alloc = sys_alloc_fn,
                       .realloc = sys_realloc_fn,
                       .free = sys_free_fn,
                       .ctx = NULL};
}
