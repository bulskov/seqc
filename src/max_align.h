#pragma once

/* Internal: the strictest fundamental alignment, for buffers and node
 * payloads that hold elements of unknown type.
 *
 * C11 spells it _Alignof(max_align_t), but MSVC's C mode does not define
 * max_align_t (only C++ <cstddef> does).  There we use what MSVC's malloc
 * guarantees instead: 16 bytes on 64-bit targets, 8 on 32-bit.  clang-cl
 * defines _MSC_VER too, but ships its own <stddef.h> with max_align_t. */

#include <stddef.h>

#if defined(_MSC_VER) && !defined(__clang__)
#define SEQC_MAX_ALIGN (2 * sizeof(void *))
#else
#define SEQC_MAX_ALIGN _Alignof(max_align_t)
#endif
