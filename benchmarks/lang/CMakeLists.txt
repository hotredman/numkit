# benchmarks/lang/CMakeLists.txt — base lang-layer micro-benches (src/lang).
# Contributes sources to the single numkit_bench target (see ../CMakeLists.txt).
target_sources(numkit_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/binaryops_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/matmul_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/manipulation_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sort_bench.cpp
)
