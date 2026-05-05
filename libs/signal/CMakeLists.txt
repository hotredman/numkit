# libs/signal/CMakeLists.txt
#
# Signal Processing Toolbox. Depends on builtin (base layer).

add_subdirectory(src)  # exports SIGNALLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${SIGNALLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/convolution/convolution.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/convolution/extras.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/digital_filtering/filter.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/digital_filtering/sosfilt.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/digital_filtering/spec_driven.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/filter_analysis/frequency_response.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/filter_analysis/predicates.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/filter_analysis/responses.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/filter_analysis/unwrap.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/filter_design/filter_design.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/filter_implementation/conversions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/filter_implementation/conversions_extras.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/measurements/dbconv.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/measurements/findpeaks.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/measurements/signal_stats.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/multirate/extras.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/multirate/multirate.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/smoothing/medfilt.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/smoothing/sgolay.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/spectral_analysis/periodogram_pwelch.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/time_frequency/spectrogram.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/transforms/dct.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/transforms/extras.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/transforms/fft.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/transforms/goertzel.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/transforms/hilbert.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/transforms/transform_helpers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/waveform_generation/waveform_generation.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/signal/windows/windows.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
