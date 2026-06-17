# benchmarks/interpreter/CMakeLists.txt — VM / interpreter execution benches.
#
# These measure the cost of running M-code through the engine itself
# (native C++ vs bytecode VM vs TreeWalker), as opposed to the math/ and lang/
# benches which time C++ kernels directly. Contributes sources to numkit_bench.
target_sources(numkit_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/iir_filter_bench.cpp
)
