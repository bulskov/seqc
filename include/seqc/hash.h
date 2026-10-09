#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* PSL above this triggers an automatic resize to protect against uint8_t
 * overflow (max storable PSL is 255). Indicates a degenerate hash function. */
#define HASH_PSL_THRESHOLD 128

/* Default hash and equality functions suitable for any fixed-size key type */
size_t hash_fnv1a(const void *key, size_t key_size);
bool hash_eq_bytes(const void *a, const void *b, size_t key_size);

/* NUL-terminated C-string keys: the key is a (char *), key_size is
 * sizeof(char *).  For string_t keys use string_hash / string_key_eq
 * (seqc/string.h). */
size_t hash_cstr(const void *key, size_t key_size);
bool hash_eq_cstr(const void *a, const void *b, size_t key_size);
