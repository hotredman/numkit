# toolboxes/wavelet/CMakeLists.txt
#
# Wavelet Toolbox. Depends on builtin.

add_subdirectory(src)  # exports WAVELETLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${WAVELETLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/wavelet/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/wavelet/denoise/denoise.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/wavelet/dwt/dwt.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/wavelet/dwt/dwt2.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/wavelet/dwt/multilevel.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/wavelet/filter/wfilters.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/wavelet/shape/shape.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/wavelet/swt/swt.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
