# libs/audio/CMakeLists.txt
#
# Audio Toolbox. Currently scale conversions only; spectral shape
# descriptors / mel-spectrogram / mfcc / pitch land in subsequent
# cycles per PROGRESS.md ## Audio section.

add_subdirectory(src)  # exports AUDIOLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${AUDIOLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/audio/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/audio/scale/freq_scales.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/audio/spectral/shape_descriptors.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/audio/spectral/melspec_delta.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
