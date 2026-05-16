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

# bundle arena_allocator dependency (merge into the same include/ and lib/)
cp -r libs/arena_allocator/include/. "$STAGE/$NAME/include/"
cp -r libs/arena_allocator/lib/.     "$STAGE/$NAME/lib/"

# bundle arena_allocator docs and fix the cross-reference link for the dist
mkdir -p "$STAGE/$NAME/docs/arena_allocator"
cp  libs/arena_allocator/README.md   "$STAGE/$NAME/docs/arena_allocator/"
cp -r libs/arena_allocator/docs/.    "$STAGE/$NAME/docs/arena_allocator/"
sed -i 's|\.\./libs/arena_allocator/README\.md|arena_allocator/README.md|' \
    "$STAGE/$NAME/docs/arena.md"

# dist/ directory
rm -rf "dist/$NAME"
mkdir -p dist
cp -r "$STAGE/$NAME" "dist/$NAME"

# zip archive
(cd "$STAGE" && zip -r "${OLDPWD}/${NAME}.zip" "$NAME")

echo "Created dist/$NAME/  and  ${NAME}.zip"
