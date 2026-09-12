# toolboxes/wavelet/CMakeLists.txt
#
# Wavelet Toolbox. Depends on builtin.

add_subdirectory(src)  # exports WAVELETLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${WAVELETLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/wavelet/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/wavelet/denoise/denoise.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/wavelet/dwt/dwt.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/wavelet/dwt/dwt2.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/wavelet/dwt/multilevel.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/wavelet/filter/wfilters.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/wavelet/shape/shape.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/wavelet/swt/swt.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
