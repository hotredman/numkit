# toolboxes/signal/CMakeLists.txt
#
# Signal Processing Toolbox. Depends on builtin (base layer).

add_subdirectory(src)  # exports SIGNALLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${SIGNALLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/convolution/convolution.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/convolution/extras.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/digital_filtering/filter.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/digital_filtering/sosfilt.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/digital_filtering/spec_driven.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/filter_analysis/frequency_response.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/filter_analysis/predicates.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/filter_analysis/responses.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/filter_analysis/unwrap.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/filter_design/filter_design.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/filter_implementation/conversions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/filter_implementation/conversions_extras.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/measurements/dbconv.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/measurements/findpeaks.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/measurements/signal_stats.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/multirate/extras.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/multirate/multirate.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/smoothing/medfilt.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/smoothing/sgolay.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/spectral_analysis/periodogram_pwelch.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/time_frequency/spectrogram.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/transforms/dct.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/transforms/extras.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/transforms/fft.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/transforms/goertzel.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/transforms/hilbert.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/transforms/transform_helpers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/waveform_generation/waveform_generation.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/signal/windows/windows.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
