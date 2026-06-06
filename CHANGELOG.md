# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- String/stdio interop: `STRING_FMT` and `STRING_ARG` macros for printing a
  `String` via `printf`'s `"%.*s"` form without allocating; a new
  `seqc/string_io.h` header with binary-safe `string_fwrite` / `string_print`
  / `string_println` (keeps `<stdio.h>` out of the core string type); and
  `string_to_cstr_buf` for NUL-terminating into a caller-supplied stack buffer.
- ASan/UBSan/LeakSanitizer build mode in `test.sh` (`./test.sh asan`).
- OOM-path tests across the container iterators, driven by a fault-injecting
  test allocator (`test/oom_alloc.h`).

### Changed

- Consume `arena_allocator` via CMake `FetchContent` instead of a vendored
  binary.

### Fixed

- Harden every iterator constructor against allocation failure: on OOM they
  now return an empty iterator (`it.next == NULL`) rather than dereferencing
  NULL or leaking partially-built state. Covers `vec`, `slice`, `list`,
  `dlist`, `queue`, `ringbuf`, `set`, `hashmap`, `bstree`, `avl`, `omap`,
  `string`, and the iterator combinators.
- Guard capacity-doubling overflow in `queue_grow` and `rb_grow`, matching the
  existing guard in `vec_push`.
- Memory bugs surfaced by code review and the sanitizers.

## [1.0.0] - 2026-05-16

### Added

- Initial release: an arena-based, generic (`void *` + `elem_size`) container
  and iterator library for C.
- Containers: `vec`, `slice`, `list`, `dlist`, `stack`, `queue`, `ringbuf`,
  `pqueue`, `set`, `hashmap`, `bstree`, `avl`, `omap`, and `string`.
- Lazy iterator layer with sources, adaptors (`map`, `filter`, `take`, `skip`,
  `chain`, `zip`, `enumerate`, `window`, `chunks`, `peekable`, `dedup`,
  `flat_map`, …), and terminals (`collect`, `count`, `find`, `sort`, …).
- CMake build with presets, GoogleTest-based test suite, and `publish.sh`
  packaging.
