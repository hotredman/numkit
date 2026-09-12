# toolboxes/graphics/CMakeLists.txt
#
# Plotting commands + figure / subplot management. Depends on builtin.

add_subdirectory(src)  # exports GRAPHICSLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${GRAPHICSLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/graphics/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/graphics/graphics_context.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/graphics/graphics_handle.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
