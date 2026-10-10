# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [3.1.0] - 2026-10-10

Every string result is now a plain allocation that works with any allocator,
not only with an arena.

### Added

- `strbuf_to_string(sb, allocator)` — a copy of what has been built, yours
  after `strbuf_clear` / `strbuf_destroy`.
- `strbuf_view(sb)` — the new name of `strbuf_finish`: a view, valid until the
  next change to the builder. `strbuf_view(NULL)` gives `{NULL, 0}`.
- docs: [Who owns a result](docs/string.md#who-owns-a-result).

### Fixed

- `string_replace` and `string_join` returned a view into an internal builder
  that was never released: with a malloc-style allocator the result could not
  be freed and the builder leaked. They now return a plain allocation of
  exactly `len` bytes and release the builder. An allocation failure gives
  `{NULL, 0}` instead of a partial result.

### Deprecated

- `strbuf_finish` — use `strbuf_view`, or `strbuf_to_string` for a copy. It
  still works, with a compiler warning, and is removed in 4.0.

## [3.0.0] - 2026-10-09

A consistent API: every collection names the same operation the same way.
See [docs/naming.md](docs/naming.md). **Breaking** — every program using
seqc needs the renames below; most are a search and replace.

### Changed (renamed)

| 2.x | 3.0 |
|---|---|
| `X_free` (every collection, `strbuf`) | `X_destroy` |
| `iter_drop`, the `iter_t` field `drop` | `iter_destroy`, field `destroy` |
| `vec_create_size` | `vec_create_with_cap` |
| `vec_get` (pointer) | `vec_get_ptr` |
| `vec_get_copy` | `vec_get` |
| `slice_get` (pointer) | `slice_get_ptr` |
| `stack_peek` (pointer) | `stack_peek_ptr` |
| `queue_peek`, `queue_back` (pointers) | `queue_front_ptr`, `queue_back_ptr` |
| `list_front/back`, `dlist_front/back` (pointers) | `…_front_ptr`, `…_back_ptr` |
| `avl_min/max`, `bstree_min/max` (pointers) | `…_min_ptr`, `…_max_ptr` |
| `ringbuf_at` | `ringbuf_get` |
| `avl_insert`, `bstree_insert` | `avl_add`, `bstree_add` |
| `hashmap_delete` | `hashmap_remove` |
| `set_add_all`, `hashmap_set_all` | `set_extend`, `hashmap_extend` |
| `pqueue_build_from_vec` | `pqueue_create_from_vec` |
| `vec_contains`, `slice_contains` (predicates) | `vec_any`, `slice_any` |
| `hash_fnv1a_str`, `hash_eq_str` (`char *` keys) | `hash_cstr`, `hash_eq_cstr` |

Watch out for the names that now mean something else: `vec_get`,
`slice_get`, `stack_peek`, `list_front` and the other plain accessors used to
return a pointer and now **copy into `out`** and return a status.

### Added

- Accessor pairs everywhere — the plain name copies (not affected by later
  changes), `_ptr` returns a pointer valid until the next change:
  `slice_get`, `stack_peek`, `queue_front`, `queue_back`, `list_front/back`,
  `dlist_front/back`, `ringbuf_get_ptr`, `ringbuf_front/back` (+`_ptr`),
  `pqueue_peek_ptr`, `avl_min/max`, `bstree_min/max`, `omap_get_ptr`,
  `omap_min_key_ptr`, `omap_max_key_ptr`, `hashmap_get_ptr`.
- `X_is_empty` for `vec`, `avl`, `bstree`, `omap`, `strbuf`.
- `X_extend(x, it)` for `stack`, `queue`, `list`, `dlist`, `ringbuf`,
  `pqueue`, `avl`, `bstree`, `omap`.
- `seqc/status.h`: `seqc_status_t` in its own header.
- `docs/naming.md`: the conventions, and the guide for moving from 2.x.

### Fixed

- A `NULL` collection is `SEQC_INVALID` for every copy accessor (`ringbuf_get`
  and `pqueue_peek` said `SEQC_NOT_FOUND`); `vec_get` accepts `out == NULL`
  like the others; `hashmap_get` on a map that never held an entry is
  `SEQC_NOT_FOUND`, not `SEQC_INVALID`.
- Docs: the examples used an arena API that no longer exists
  (`arena_create`, `arena_free`); they now use `growing_arena_*`.
  `vec_get_ptr` was documented as unchecked; it returns `NULL` out of range.

## [2.4.0] - 2026-10-09

### Added

- `strbuf_clear`: empty a builder for reuse, keeping its buffer.
- `strbuf_free`: release a builder and its buffer through its allocator.

### Fixed

- `strbuf_create` returns `NULL` when the builder's buffer cannot be
  allocated, instead of returning a builder whose first append would
  dereference a NULL buffer.

## [2.3.0] - 2026-10-07

### Added

- `string_find_char` / `string_rfind_char`: byte offset of the first / last
  occurrence of a byte, or `STRING_NOT_FOUND`. Exactly `s.len` bytes are
  searched.
- `string_split_next`: an allocation-free tokenizer over a `string_t` cursor.
  Empty pieces are kept, as in `string_split_substr`.
- `string_compare_case_insensitive`: ASCII-folded ordering (to lower case, as
  `strcasecmp`), consistent with `string_equals_case_insensitive`.
- `string_hash_case_insensitive` / `string_key_eq_case_insensitive`: a
  `hash_fn` / `eq_fn` pair for hashmaps and sets with case-insensitive
  `string_t` keys.

### Fixed

- Case folding no longer depends on the C locale. `string_equals_case_insensitive`,
  `string_to_lowercase` and `string_to_uppercase` used `tolower` / `toupper`,
  which in a single-byte locale (e.g. ISO-8859-1) also fold bytes above 127 —
  bytes of UTF-8 sequences — so `string_to_lowercase` could corrupt UTF-8 text.
  Only ASCII letters are folded now.
- `string_to_lowercase` / `string_to_uppercase` return `{NULL, 0}` when the
  allocation fails instead of writing through a NULL pointer.

### Changed

- `.clang-format`: `InsertBraces: true` — braces around every `if` / `else` /
  `for` / `while` body.

## [2.2.3] - 2026-10-07

### Changed

- Bump the pinned `arena_allocator` dependency from v1.1.4 to v1.1.5. The
  library code is unchanged; arena's tests now pass on Apple Silicon.
- CI: GitHub Actions on Linux (gcc, clang, ASan), Windows (MSVC `cl`,
  clang-cl) and macOS.

### Fixed

- Build with MSVC (`cl`), which does not define `max_align_t` in C. Every
  internal `_Alignof(max_align_t)` now goes through `SEQC_MAX_ALIGN`
  (`src/max_align.h`), which falls back to MSVC's malloc alignment
  (`2 * sizeof(void *)`) under `cl` and is unchanged elsewhere.
- Tests: `string_io_test` builds and passes on Windows (no `<unistd.h>`;
  stdout captured in binary mode).

## [2.2.2] - 2026-10-07

### Changed

- Bump the pinned `arena_allocator` dependency from v1.1.3 to v1.1.4. The
  library code is unchanged; arena now gives clang-cl MSVC-style warning flags.

### Fixed

- Build: select warning flags with CMake's `MSVC` variable instead of the
  compiler id. clang-cl reports itself as Clang, so it got `-Wall`, which in
  clang-cl means `-Weverything` and buried consumers' Windows builds in
  hundreds of warnings. It now gets `/W4`, like MSVC.

## [2.2.1] - 2026-10-07

### Fixed

- Build: seqc's own test suite is only configured when seqc is the top-level
  project. Consumers pulling seqc in through FetchContent or add_subdirectory no
  longer fetch ctt or get seqc's tests in their ctest run, and no longer need to
  force `BUILD_TESTING=OFF` before fetching seqc. At top level,
  `-DBUILD_TESTING=OFF` still skips the tests.

## [2.2.0] - 2026-10-06

### Changed

- Tests: switch the test suite from GoogleTest to
  [ctt](https://github.com/bulskov/ctt) (v0.3.1). The tests now build as plain
  C11; seqc no longer needs a C++ compiler. Override `SEQC_CTT_GIT_TAG` to pin
  a different ctt version.

## [2.1.0] - 2026-10-06

### Added

- `string_equals_case_insensitive`: like `string_equals`, but ASCII letters
  compare without regard to case.
- `strbuf_len`: number of bytes appended to a builder so far.
- Docs: `docs/arena.md` now covers the bounded arenas. `fixed_arena_t` caps a
  fully-committed region; `virtual_arena_t` reserves an address range and
  commits pages on demand, giving growing-arena behaviour with a hard ceiling.
  Both return `NULL` past the limit, which seqc reports as `SEQC_OOM`. Adds the
  full `fixed_arena_*` / `virtual_arena_*` API, `arena_stats_t`, and
  `growing_arena_stats`.
- Docs: a "Bounded allocation (`--max-mem`)" pattern showing that a memory
  ceiling is purely a choice of arena at start-up — every collection stores its
  `allocator_t` by value, so no seqc API takes or needs a limit parameter. Notes
  that a bump arena strands each superseded buffer on realloc, so the ceiling
  bounds total bytes handed out rather than live data.
- Docs: `growing_arena_reset_full`, previously undocumented.

### Changed

- Bump the pinned `arena_allocator` dependency from v1.1.1 to v1.1.3. The
  library code is unchanged; arena's own tests moved to ctt and its README now
  documents FetchContent usage.

### Fixed

- `string_copy` returns an empty string on allocation failure instead of
  passing `NULL` to `memcpy`.
- Docs: `growing_arena_reset` was described as reusing all committed blocks and
  not releasing memory to the OS. It frees every block except the head.
- Docs: `scratch_t` was described as a checkpoint into `growing_arena_t`. It
  works with all three arenas; the per-arena `*_scratch_begin` entry points are
  now listed.

## [2.0.1] - 2026-06-07

### Fixed

- CMake: use `PROJECT_SOURCE_DIR` instead of `CMAKE_SOURCE_DIR` for the public
  include directory and the header install source. When seqc is consumed via
  `FetchContent`/`add_subdirectory`, `CMAKE_SOURCE_DIR` points at the top-level
  consumer project rather than seqc, so its headers were not found at build
  time. `PROJECT_SOURCE_DIR` resolves to seqc's own root in both standalone and
  subproject builds.

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
