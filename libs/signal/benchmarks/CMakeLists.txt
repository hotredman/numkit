target_sources(numkit_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fft_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/conv_bench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp_bench.cpp
)

# fft_bench probes internal kernels (FftKernels.hpp, fillFftTwiddles)
# to attribute time between the public fft() wrapper and the inner FFT
# loop. Those headers live in libs/signal/src/transforms/backends/.
target_include_directories(numkit_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
