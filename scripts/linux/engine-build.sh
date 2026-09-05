#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

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

case "${1:-}" in
    --wasm)
        if ! command -v emcmake &>/dev/null; then
            echo "emcmake not found. Run: source ~/emsdk/emsdk_env.sh"
            exit 1
        fi
        cmake --preset=wasm-release
        cmake --build --preset=wasm-release
        echo "WASM build OK (wasm-release)"
        ;;
    --debug)
        cmake --preset=linux-debug "${EXTRA_FLAGS[@]}"
        cmake --build --preset=linux-debug -j"$(nproc)"
        echo "Build OK (linux-debug)"
        ;;
    --portable)
        cmake --preset=linux-portable "${EXTRA_FLAGS[@]}"
        cmake --build --preset=linux-portable -j"$(nproc)"
        echo "Build OK (linux-portable)"
        ;;
    *)
        cmake --preset=linux-release "${EXTRA_FLAGS[@]}"
        cmake --build --preset=linux-release -j"$(nproc)"
        echo "Build OK (linux-release)"
        ;;
esac
