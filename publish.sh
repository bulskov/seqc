#!/usr/bin/env bash
# Build a release archive and dist directory.
# Usage: ./publish.sh [output-name]
# Output: dist/<name>/  and  <name>.zip
set -e

VERSION=$(cat "$(dirname "$0")/VERSION")
NAME=${1:-"seqc-${VERSION}"}
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

cmake --preset release -DBUILD_TESTING=OFF
cmake --build --preset release
cmake --install build/release --prefix "$STAGE/$NAME"

cp README.md "$STAGE/$NAME/"
cp -r docs   "$STAGE/$NAME/"

# Locate the arena_allocator dependency that FetchContent resolved.  These
# cache entries point at the source checkout and the build tree regardless of
# whether arena was cloned into _deps/ or supplied via FETCHCONTENT_SOURCE_DIR.
ARENA_SRC=$(sed -n 's/^arena_SOURCE_DIR:[^=]*=//p'   build/release/CMakeCache.txt)
ARENA_BUILD=$(sed -n 's/^arena_BINARY_DIR:[^=]*=//p' build/release/CMakeCache.txt)

# bundle arena_allocator dependency (merge into the same include/ and lib/);
# the generated version.h lives in the build tree, the rest in the source tree
cp -r "$ARENA_SRC/include/."                       "$STAGE/$NAME/include/"
cp    "$ARENA_BUILD/include/arena/version.h"       "$STAGE/$NAME/include/arena/"
rm -f "$STAGE/$NAME/include/arena/version.h.in"  # ship the generated header only
cp    "$ARENA_BUILD/libarena.a"                    "$STAGE/$NAME/lib/"

# bundle arena_allocator docs and fix the cross-reference link for the dist
mkdir -p "$STAGE/$NAME/docs/arena_allocator"
cp  "$ARENA_SRC/README.md"   "$STAGE/$NAME/docs/arena_allocator/"
cp -r "$ARENA_SRC/docs/."    "$STAGE/$NAME/docs/arena_allocator/"
sed -i 's|https://github.com/bulskov/arena_allocation|arena_allocator/README.md|' \
    "$STAGE/$NAME/docs/arena.md"

# dist/ directory
rm -rf "dist/$NAME"
mkdir -p dist
cp -r "$STAGE/$NAME" "dist/$NAME"

# zip archive
(cd "$STAGE" && zip -r "${OLDPWD}/${NAME}.zip" "$NAME")

echo "Created dist/$NAME/  and  ${NAME}.zip"
