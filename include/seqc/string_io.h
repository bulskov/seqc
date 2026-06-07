#pragma once

/* Optional stdio interop for string_t.  Kept separate from seqc/string.h so the
 * core string type carries no <stdio.h> / FILE * dependency.  These helpers
 * are binary-safe: they write exactly s.len bytes and tolerate embedded NULs,
 * unlike the STRING_FMT / "%.*s" approach. */

#include <stddef.h>
#include <stdio.h>

#include "seqc/string.h"

/* Write s.len bytes of s to stream f (no NUL terminator needed).
 * Returns the number of bytes written, as fwrite would.  Returns 0 for a
 * NULL stream or an empty string. */
size_t string_fwrite(string_t s, FILE *f);

/* Write s to stdout.  Returns the number of bytes written. */
size_t string_print(string_t s);

/* Write s followed by '\n' to stdout.  Returns the number of bytes written,
 * including the newline. */
size_t string_println(string_t s);
