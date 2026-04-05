#!/bin/bash
set -e

cd "$(dirname "$0")"

if [ "${1:-}" = "--wasm" ]; then
    if ! command -v emcmake &>/dev/null; then
        echo "emcmake not found. Run: source ~/emsdk/emsdk_env.sh"
        exit 1
    fi
    emcmake cmake -B build-wasm -DMLAB_BUILD_REPL=ON
    cmake --build build-wasm -j$(nproc)
    echo "WASM build OK"
else
    cmake -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build -j$(nproc)
    echo "Build OK"
fi
