set(IMAGELIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/arithmetic/arithmetic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type_convert/type_convert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/color/color.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/filter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrast/contrast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/morph/morph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/region/region.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/object/object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/quality/quality.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/transform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/stb_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/geom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/segment/segment.cpp
)

set(IMAGELIB_SOURCES ${IMAGELIB_SOURCES} PARENT_SCOPE)
