target_sources(numkit_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/matmul_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sort_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/binaryops_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/elementwise_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/reductions_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/manipulation_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/setops_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/interp_bench.cpp
)
