set(SIGNALLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    # waveform_generation/
    ${CMAKE_CURRENT_SOURCE_DIR}/waveform_generation/waveform_generation.cpp
    # filter_design/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/filter_design.cpp
    # filter_analysis/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/frequency_response.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/unwrap.cpp
    # digital_filtering/
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/filter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/sosfilt.cpp
    # filter_implementation/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_implementation/conversions.cpp
    # multirate/
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate/multirate.cpp
    # convolution/
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/convolution.cpp
    # transforms/
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/fft.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/hilbert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/dct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/goertzel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/transform_helpers.cpp
    # smoothing/
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/sgolay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/medfilt.cpp
    # measurements/
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/findpeaks.cpp
    # spectral_analysis/
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/periodogram_pwelch.cpp
    # time_frequency/
    ${CMAKE_CURRENT_SOURCE_DIR}/time_frequency/spectrogram.cpp
    # windows/
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/windows.cpp
)

# Backend-split FFT kernel — the SIMD path itself splits across
# several TUs (one per radix family) so MSVC's per-TU inliner
# budget isn't shared across butterflies; see
# feedback_fft_msvc_limits memory for the rationale.
if(NUMKIT_WITH_SIMD)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/fft_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/fft_r2_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/fft_r4_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/fft_stockham_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/fft_r2_soa_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/fft_r4_soa_simd.cpp)
else()
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/fft_portable.cpp)
endif()

set(SIGNALLIB_SOURCES ${SIGNALLIB_SOURCES} PARENT_SCOPE)
