# benchmarks/CMakeLists.txt — single m_bench executable
#
# Mirrors the m_tests layout: one binary, one source file per hot path.
# benchmark::benchmark_main provides main() that discovers BENCHMARK()
# macros from every TU linked in, so no custom main.cpp is needed.
#
# Enabled by NUMKIT_M_BUILD_BENCHMARKS=ON at configure time. Run examples:
#   ./m_bench                                  # all benches
#   ./m_bench --benchmark_filter=BM_Fft.*      # fft family only
#   ./m_bench --benchmark_min_time=0.5s        # shorter / longer runs
#   ./m_bench --benchmark_out=results.json \
#             --benchmark_out_format=json      # machine-readable output

add_executable(m_bench
    fft_bench.cpp
    matmul_bench.cpp
    conv_bench.cpp
    elementwise_bench.cpp
    binaryops_bench.cpp
    sort_bench.cpp
)

target_link_libraries(m_bench
    PRIVATE
        numkit::m
        benchmark::benchmark_main
)

# Some bench TUs (fft_bench) probe internal kernels — fftRadix2Impl,
# fillFftTwiddles — to attribute time between the public wrapper and
# the inner FFT loop without going through the pmr allocator. The
# headers live in libs/dsp/src, so expose that path to the bench.
target_include_directories(m_bench PRIVATE
    "${CMAKE_SOURCE_DIR}/libs/dsp/src"
)

if(EMSCRIPTEN)
    # Emscripten ships a Node-runnable .js loader + .wasm sidecar when the
    # output has a .js suffix. Bench run via:  node m_bench.js
    set_target_properties(m_bench PROPERTIES SUFFIX ".js")
    target_compile_options(m_bench PRIVATE -fexceptions)
    target_link_options(m_bench PRIVATE
        -fexceptions
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s INITIAL_MEMORY=134217728"
        "SHELL:-s STACK_SIZE=4194304"
        "SHELL:-s EXIT_RUNTIME=1"
        "SHELL:-s NODERAWFS=1"
        "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
    )
endif()
