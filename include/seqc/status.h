#pragma once

/* seqc_status_t — the result of every seqc operation that can fail.  Its
 * own header so that seqc/slice.h and seqc/iter.h can both use it without
 * including each other. */

typedef enum
{
    SEQC_OK = 0,    /* operation succeeded / element found    */
    SEQC_NOT_FOUND, /* element or key is absent               */
    SEQC_DUPLICATE, /* element already present (no-op insert) */
    SEQC_OOM,       /* allocator returned NULL                */
    SEQC_INVALID,   /* NULL or otherwise invalid argument     */
} seqc_status_t;
