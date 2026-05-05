# libs/image/CMakeLists.txt
#
# Image Processing Toolbox. Depends on builtin + signal (DSP primitives).

add_subdirectory(src)  # exports IMAGELIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${IMAGELIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/arithmetic/arithmetic.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/color/color.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/contrast/contrast.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/filter/filter.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/geom/geom.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/io/io.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/morph/morph.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/object/object.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/quality/quality.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/region/region.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/segment/segment.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/transform/transform.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/image/type_convert/type_convert.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
