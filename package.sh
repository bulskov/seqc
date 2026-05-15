#!/usr/bin/env bash
# Build a release archive containing the compiled static library and headers.
# Usage: ./package.sh [output-name]
# Output: <output-name>.zip  (default: seqc)
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

(cd "$STAGE" && zip -r "${OLDPWD}/${NAME}.zip" "$NAME")

echo "Created ${NAME}.zip"
