# benchmarks/CMakeLists.txt — owner of the single numkit_bench executable.
#
# Mirrors the numkit_gtest pattern: this directory creates the target;
# per-lib toolboxes/<name>/benchmarks/CMakeLists.txt contributes sources via
#   target_sources(numkit_bench PRIVATE ...)
# (and may also extend target_include_directories — fft_bench needs
# toolboxes/signal/src for kernel internals).
#
# Enabled by NUMKIT_BUILD_BENCHMARKS=ON at configure time. Run examples:
#   ./numkit_bench                                  # all benches
#   ./numkit_bench --benchmark_filter=BM_Fft.*      # fft family only
#   ./numkit_bench --benchmark_min_time=0.5s        # shorter / longer runs
#   ./numkit_bench --benchmark_out=results.json \
#             --benchmark_out_format=json           # machine-readable output

add_executable(numkit_bench)

target_link_libraries(numkit_bench PRIVATE
    numkit::numkit
    benchmark::benchmark_main
)

# Pull in per-lib benchmark sources. Each lib that contributes benches
# has toolboxes/<name>/benchmarks/CMakeLists.txt with target_sources() calls.
foreach(_bench_lib signal stats image comm control wavelet graphics io optim linalg)
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/toolboxes/${_bench_lib}/benchmarks/CMakeLists.txt)
        add_subdirectory(${CMAKE_SOURCE_DIR}/src/toolboxes/${_bench_lib}/benchmarks
                         ${CMAKE_BINARY_DIR}/toolboxes/${_bench_lib}/benchmarks)
    endif()
endforeach()
# Base-layer (math / lang) + interpreter/VM (core) benches — co-located under
# src/<layer>/benchmarks/, same as the per-toolbox benches above and the
# src/<module>/tests/ layout.
foreach(_bench_layer math lang ops core codegen)
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/${_bench_layer}/benchmarks/CMakeLists.txt)
        add_subdirectory(${CMAKE_SOURCE_DIR}/src/${_bench_layer}/benchmarks
                         ${CMAKE_BINARY_DIR}/${_bench_layer}/benchmarks)
    endif()
endforeach()

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
