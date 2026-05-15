#!/usr/bin/env bash
set -e

CLEAN=0
PRESET="debug"

for arg in "$@"; do
    case "$arg" in
        clean) CLEAN=1 ;;
        *)     PRESET="$arg" ;;
    esac
done

if [[ $CLEAN -eq 1 ]]; then
    rm -rf "build/$PRESET"
fi

cmake --preset "$PRESET"
cmake --build --preset "$PRESET"
