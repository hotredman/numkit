set(IMAGELIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/arithmetic/arithmetic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type_convert/type_convert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/color/color.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/filter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrast/contrast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/morph/morph.cpp
)

set(IMAGELIB_SOURCES ${IMAGELIB_SOURCES} PARENT_SCOPE)
