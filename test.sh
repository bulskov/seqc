#!/usr/bin/env bash
set -e

# Usage: ./test.sh [debug|asan]
#   debug (default) — plain build + ctest
#   asan            — AddressSanitizer/UBSan/LeakSanitizer build + ctest

MODE="${1:-debug}"

# Ensure malloc/free have not snuck into library code
# Match the bare calls: malloc( realloc( calloc( free( — but not allocator.free( etc.
# arena.c is exempt: it intentionally wraps malloc/realloc/free for sys_allocator().
if grep -Prn '(?<![.\w])(malloc|free|realloc|calloc)\s*\(' src/ --include='*.c' --include='*.h' \
    --exclude='arena.c'; then
  echo "ERROR: heap allocation found in src/ — use arena or scratch instead" >&2
  exit 1
fi

case "$MODE" in
  debug)
    cmake --preset debug
    cmake --build --preset debug
    ctest --preset debug
    ;;
  asan)
    # Separate build dir so it does not clobber the plain debug cache.
    # detect_stack_use_after_return catches iterators that yield pointers
    # into a returned stack frame; leak detection is on by default on Linux.
    cmake -S . -B build/asan -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DSEQC_SANITIZERS=ON
    cmake --build build/asan
    ASAN_OPTIONS=detect_stack_use_after_return=1:detect_leaks=1 \
        ctest --test-dir build/asan --output-on-failure
    ;;
  *)
    echo "usage: $0 [debug|asan]" >&2
    exit 1
    ;;
esac
