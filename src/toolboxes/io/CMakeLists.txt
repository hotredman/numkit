# toolboxes/io/CMakeLists.txt
#
# File I/O: low-level + CSV / readtable + spreadsheets + path helpers.
# (Workspace save/load moved to runtime.) Depends on builtin.

add_subdirectory(src)  # exports IOLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${IOLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/io/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/io/file_io/fileio.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/io/paths/paths.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/io/text/csv.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/io/text/extras.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
