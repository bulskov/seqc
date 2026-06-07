#include "seqc/stack.h"
#include "seqc/vec.h"

#include <string.h>

struct seqc_stack_t
{
    vec_t *vec;
    allocator_t allocator;
};

seqc_stack_t *stack_create(size_t elem_size, allocator_t allocator)
{
    seqc_stack_t *s = mem_alloc(allocator, sizeof(seqc_stack_t), _Alignof(seqc_stack_t));
    if (!s)
        return NULL;
    s->vec = vec_create(elem_size, allocator);
    if (!s->vec)
    {
        mem_free(allocator, s, sizeof(seqc_stack_t));
        return NULL;
    }
    s->allocator = allocator;
    return s;
}

seqc_status_t stack_push(seqc_stack_t *s, const void *elem)
{
    if (!s)
        return SEQC_INVALID;
    return vec_push(s->vec, elem);
}

seqc_status_t stack_pop(seqc_stack_t *s, void *out)
{
    if (!s)
        return SEQC_INVALID;
    return vec_pop(s->vec, out);
}

void *stack_peek(const seqc_stack_t *s)
{
    if (!s || vec_len(s->vec) == 0)
        return NULL;
    return vec_get(s->vec, vec_len(s->vec) - 1);
}

bool stack_is_empty(const seqc_stack_t *s)
{
    return !s || vec_len(s->vec) == 0;
}

size_t stack_len(const seqc_stack_t *s)
{
    return s ? vec_len(s->vec) : 0;
}

iter_t stack_iter(const seqc_stack_t *s)
{
    if (!s)
        return (iter_t){0};
    return vec_iter(s->vec);
}

iter_t stack_iter_rev(const seqc_stack_t *s)
{
    if (!s)
        return (iter_t){0};
    return vec_iter_rev(s->vec);
}

void stack_clear(seqc_stack_t *s)
{
    if (s)
        vec_clear(s->vec);
}

void stack_free(seqc_stack_t *s)
{
    if (!s)
        return;
    vec_free(s->vec);
    allocator_t al = s->allocator;
    mem_free(al, s, sizeof(seqc_stack_t));
}
