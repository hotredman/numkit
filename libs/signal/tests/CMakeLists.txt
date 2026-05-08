target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/signal_core_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows_extras_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/waveform_extras_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resample_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transform_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fft_public_api_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dspgaps_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sos_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sgolay_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spec_driven_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/conversions_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution_extras_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate_extras_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms_extras_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows_sflag_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hilbert_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enbw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/envelope_test.cpp
)

# fft_public_api_test reaches into libs/signal/src/transforms/backends/
# fft_kernels.hpp to call the Stockham/r2 dispatchers directly for parity
# tests. Expose that include path only for the test target (the public
# signal API doesn't need it).
target_include_directories(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
