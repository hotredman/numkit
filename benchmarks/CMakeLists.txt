# benchmarks/CMakeLists.txt — single numkit_bench executable
#
# Mirrors the numkit_tests layout: one binary, one source file per hot path.
# benchmark::benchmark_main provides main() that discovers BENCHMARK()
# macros from every TU linked in, so no custom main.cpp is needed.
#
# Enabled by NUMKIT_BUILD_BENCHMARKS=ON at configure time. Run examples:
#   ./numkit_bench                                  # all benches
#   ./numkit_bench --benchmark_filter=BM_Fft.*      # fft family only
#   ./numkit_bench --benchmark_min_time=0.5s        # shorter / longer runs
#   ./numkit_bench --benchmark_out=results.json \
#             --benchmark_out_format=json      # machine-readable output

add_executable(numkit_bench
    fft_bench.cpp
    matmul_bench.cpp
    conv_bench.cpp
    elementwise_bench.cpp
    binaryops_bench.cpp
    sort_bench.cpp
    # Phase 10 — sweep across the parity-expansion functions (Phase 1-9).
    reductions_bench.cpp
    manipulation_bench.cpp
    setops_bench.cpp
    dsp_bench.cpp
    interp_bench.cpp
)

target_link_libraries(numkit_bench
    PRIVATE
        numkit::numkit
        benchmark::benchmark_main
)

# Some bench TUs (fft_bench) probe internal kernels — fftRadix2Impl,
# fillFftTwiddles — to attribute time between the public wrapper and
# the inner FFT loop without going through the pmr allocator. The
# headers live in libs/signal/src, so expose that path to the bench.
target_include_directories(numkit_bench PRIVATE
    "${CMAKE_SOURCE_DIR}/libs/signal/src"
)

if(EMSCRIPTEN)
    # Emscripten ships a Node-runnable .js loader + .wasm sidecar when the
    # output has a .js suffix. Bench run via:  node numkit_bench.js
    set_target_properties(numkit_bench PROPERTIES SUFFIX ".js")
    target_compile_options(numkit_bench PRIVATE -fexceptions)
    target_link_options(numkit_bench PRIVATE
        -fexceptions
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s INITIAL_MEMORY=134217728"
        "SHELL:-s STACK_SIZE=4194304"
        "SHELL:-s EXIT_RUNTIME=1"
        "SHELL:-s NODERAWFS=1"
        "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
    )
endif()
