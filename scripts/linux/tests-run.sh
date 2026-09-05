#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

echo "=== Building test target (numkit_gtest) ==="
if [[ ! -f "build/linux/release/CMakeCache.txt" ]]; then
    echo "Configuring linux-release preset..."
    EXTRA_FLAGS=()
    if [[ -d "build/linux/release/_deps/highway-src" ]]; then
        EXTRA_FLAGS+=("-DFETCHCONTENT_SOURCE_DIR_HIGHWAY=$(pwd)/build/linux/release/_deps/highway-src")
    elif [[ -d "build/windows/release/_deps/highway-src" ]]; then
        EXTRA_FLAGS+=("-DFETCHCONTENT_SOURCE_DIR_HIGHWAY=$(pwd)/build/windows/release/_deps/highway-src")
    fi
    if [[ -d "build/linux/release/_deps/googletest-src" ]]; then
        EXTRA_FLAGS+=("-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$(pwd)/build/linux/release/_deps/googletest-src")
    elif [[ -d "build/windows/release/_deps/googletest-src" ]]; then
        EXTRA_FLAGS+=("-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$(pwd)/build/windows/release/_deps/googletest-src")
    fi
    cmake --preset=linux-release "${EXTRA_FLAGS[@]}"
fi
cmake --build --preset=linux-release --target numkit_gtest -j"$(nproc)"

if [[ "${1:-}" == "--build-only" ]]; then
    echo "Build completed successfully (--build-only specified)."
    exit 0
fi

TEST_EXE="build/linux/release/tests/gtest/numkit_gtest"
if [[ ! -f "$TEST_EXE" ]]; then
    echo "ERROR: Could not find test runner at $TEST_EXE"
    exit 1
fi

echo
echo "=== Running tests ==="
"$TEST_EXE" "$@"
