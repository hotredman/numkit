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
    sort_bench.cpp
)

target_link_libraries(m_bench
    PRIVATE
        numkit::m
        benchmark::benchmark_main
)
