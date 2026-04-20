#!/bin/bash
set -e

cd "$(dirname "$0")"

case "${1:-}" in
    --wasm)
        if ! command -v emcmake &>/dev/null; then
            echo "emcmake not found. Run: source ~/emsdk/emsdk_env.sh"
            exit 1
        fi
        cmake --preset=browser
        cmake --build --preset=browser
        echo "WASM build OK"
        ;;
    --fast)
        cmake --preset=desktop-fast
        cmake --build --preset=desktop-fast -j$(nproc)
        echo "Build OK (desktop-fast)"
        ;;
    *)
        cmake --preset=portable
        cmake --build --preset=portable -j$(nproc)
        echo "Build OK (portable)"
        ;;
esac
