# toolboxes/audio/CMakeLists.txt
#
# Audio Toolbox. Currently scale conversions only; spectral shape
# descriptors / mel-spectrogram / mfcc / pitch land in subsequent
# cycles per PROGRESS.md ## Audio section.

add_subdirectory(src)  # exports AUDIOLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${AUDIOLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/audio/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/audio/scale/freq_scales.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/audio/spectral/shape_descriptors.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/audio/spectral/melspec_delta.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/audio/spectral/cepstral.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/audio/features/pitch_harmonics.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
