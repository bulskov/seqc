# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.0.0] - 2026-06-07

### Added

- `string_split_any(s, set, al)`: split on **any** single character in `set`,
  with each matching character acting as its own boundary. Empty tokens are
  kept, so whitespace-style tokenisation is `string_split_any` composed with
  `iter_filter` to drop the empties.

### Changed

- **BREAKING:** all public types are renamed from `TitleCase` to lowercase
  `snake_case_t`, matching the C standard library and the `arena_allocator`
  dependency (no more mixing `String`/`Iter` with `allocator_t` in one
  signature). Examples: `String`→`string_t`, `Iter`→`iter_t`, `Vec`→`vec_t`,
  `List`→`list_t`, `HashMap`→`hashmap_t`, `BSTree`→`bstree_t`, `AVLTree`→`avl_t`,
  `SeqcStatus`→`seqc_status_t`, `MapEntry`→`map_entry_t`. The `Stack` type
  becomes **`seqc_stack_t`** (not `stack_t`, which POSIX reserves in
  `<signal.h>`).
- **BREAKING:** `StringBuilder` is renamed to `strbuf_t` and its `sb_*`
  functions to `strbuf_*` (e.g. `sb_append` → `strbuf_append`).
- **BREAKING:** `string_split` is renamed to `string_split_substr` to make the
  match semantics explicit alongside the new `string_split_any`. The behaviour
  is otherwise unchanged (split on a contiguous substring, empty tokens kept).
- **BREAKING:** splitting on an empty delimiter/set now yields the input as a
  single token instead of emitting each character. Use `string_chars` for
  per-character iteration.

## [1.1.0] - 2026-06-07

### Added

- string_t/stdio interop: `STRING_FMT` and `STRING_ARG` macros for printing a
  `string_t` via `printf`'s `"%.*s"` form without allocating; a new
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
