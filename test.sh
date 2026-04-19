#!/bin/bash
set -e

cd "$(dirname "$0")"

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

cd build
./tests/m_tests "$@"
