# toolboxes/image/CMakeLists.txt
#
# Image Processing Toolbox. Depends on builtin + signal (DSP primitives).

add_subdirectory(src)  # exports IMAGELIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${IMAGELIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/arithmetic/arithmetic.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/color/color.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/contrast/contrast.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/filter/filter.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/geom/geom.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/io/io.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/morph/morph.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/object/object.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/quality/quality.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/region/region.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/segment/segment.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/transform/transform.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/image/type_convert/type_convert.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
