# benchmarks/math/CMakeLists.txt — base math-layer micro-benches (src/math).
# Contributes sources to the single numkit_bench target (see ../CMakeLists.txt).
target_sources(numkit_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/elementwise_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/interp_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/reductions_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/setops_bench.cpp
)
