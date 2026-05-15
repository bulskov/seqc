#include "seqc/stack.h"
#include "seqc/vec.h"

#include <string.h>

struct Stack
{
    Vec *vec;
    allocator_t allocator;
};

Stack *stack_create(size_t elem_size, allocator_t allocator)
{
    Stack *s = mem_alloc(allocator, sizeof(Stack), _Alignof(Stack));
    if (!s)
        return NULL;
    s->vec = vec_create(elem_size, allocator);
    if (!s->vec)
    {
        mem_free(allocator, s, sizeof(Stack));
        return NULL;
    }
    s->allocator = allocator;
    return s;
}

SeqcStatus stack_push(Stack *s, const void *elem)
{
    if (!s)
        return SEQC_INVALID;
    return vec_push(s->vec, elem);
}

SeqcStatus stack_pop(Stack *s, void *out)
{
    if (!s)
        return SEQC_INVALID;
    return vec_pop(s->vec, out);
}

void *stack_peek(const Stack *s)
{
    if (!s || vec_len(s->vec) == 0)
        return NULL;
    return vec_get(s->vec, vec_len(s->vec) - 1);
}

bool stack_is_empty(const Stack *s)
{
    return !s || vec_len(s->vec) == 0;
}

size_t stack_len(const Stack *s)
{
    return s ? vec_len(s->vec) : 0;
}

Iter stack_iter(const Stack *s)
{
    if (!s)
        return (Iter){0};
    return vec_iter(s->vec);
}

Iter stack_iter_rev(const Stack *s)
{
    if (!s)
        return (Iter){0};
    return vec_iter_rev(s->vec);
}

void stack_clear(Stack *s)
{
    if (s)
        vec_clear(s->vec);
}

void stack_free(Stack *s)
{
    if (!s)
        return;
    vec_free(s->vec);
    allocator_t al = s->allocator;
    mem_free(al, s, sizeof(Stack));
}
