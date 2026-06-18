# src/ops/benchmarks/CMakeLists.txt — ops-layer fused SIMD kernel benches.
# Contributes sources to the single numkit_bench target (see benchmarks/CMakeLists.txt).
target_sources(numkit_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fused_bench.cpp
)
