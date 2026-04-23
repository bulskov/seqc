#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/* PSL above this triggers an automatic resize to protect against uint8_t
 * overflow (max storable PSL is 255). Indicates a degenerate hash function. */
#define HASH_PSL_THRESHOLD 128

/* Default hash and equality functions suitable for any fixed-size key type */
size_t hash_fnv1a(const void *key, size_t key_size);
bool hash_eq_bytes(const void *a, const void *b, size_t key_size);

/* String variants: key is a (char *), key_size is sizeof(char *) */
size_t hash_fnv1a_str(const void *key, size_t key_size);
bool hash_eq_str(const void *a, const void *b, size_t key_size);
