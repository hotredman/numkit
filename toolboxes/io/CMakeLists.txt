# toolboxes/io/CMakeLists.txt
#
# File I/O: low-level + CSV / readtable + spreadsheets + path helpers.
# (Workspace save/load moved to core-libs/workspace.) Depends on builtin.

add_subdirectory(src)  # exports IOLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${IOLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/file_io/fileio.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/paths/paths.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/text/csv.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/text/extras.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
