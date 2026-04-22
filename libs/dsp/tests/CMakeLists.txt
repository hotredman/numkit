target_sources(m_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/signal_core_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resample_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transform_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fft_public_api_test.cpp
)

# fft_public_api_test reaches into libs/dsp/src/backends/FftKernels.hpp
# to call the Stockham/r2 dispatchers directly for parity tests. Expose
# that include path only for the test target (the public dsp API
# doesn't need it).
target_include_directories(m_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
