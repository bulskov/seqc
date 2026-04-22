#!/usr/bin/env bash
set -e

# Ensure malloc/free have not snuck into library code
# Match the bare calls: malloc( realloc( calloc( free( — but not allocator.free( etc.
# arena.c is exempt: it intentionally wraps malloc/realloc/free for sys_allocator().
if grep -Prn '(?<![.\w])(malloc|free|realloc|calloc)\s*\(' src/ --include='*.c' --include='*.h' \
    --exclude='arena.c'; then
  echo "ERROR: heap allocation found in src/ — use arena or scratch instead" >&2
  exit 1
fi

cmake --preset debug
cmake --build --preset debug
ctest --preset debug
