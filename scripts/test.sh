#!/bin/bash
set -e

cd "$(dirname "$0")/.."

# Build + run the full gtest suite via the desktop-fast preset (the canonical
# dev config — see CLAUDE.md). ctest resolves the numkit_gtest binary regardless
# of platform layout. Extra args pass through, e.g. `test.sh -R Haart`.
cmake --preset=desktop-fast
cmake --build --preset=desktop-fast
ctest --preset=desktop-fast "$@"
