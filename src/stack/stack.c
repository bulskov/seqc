#include "stack.h"

#include <string.h>

struct Stack
{
    Vec *vec;
    Allocator allocator;
};

Stack *stack_create(size_t elem_size, Allocator allocator)
{
    Stack *s = allocator.alloc(allocator.ctx, sizeof(Stack), _Alignof(Stack));
    if (!s)
        return NULL;
    s->vec = vec_create(elem_size, allocator);
    s->allocator = allocator;
    return s;
}

void stack_push(Stack *s, const void *elem)
{
    if (!s)
        return;
    vec_push(s->vec, elem);
}

bool stack_pop(Stack *s, void *out)
{
    if (!s)
        return false;
    return vec_pop(s->vec, out);
}

void *stack_peek(const Stack *s)
{
    if (!s)
        return NULL;
    /* vec_get returns NULL when index >= len (unsigned underflow on empty) */
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
    Allocator al = s->allocator;
    if (al.free)
        al.free(al.ctx, s);
}
