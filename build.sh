#!/usr/bin/env bash
set -e

PRESET=${1:-debug}

cmake --preset "$PRESET"
cmake --build --preset "$PRESET"
