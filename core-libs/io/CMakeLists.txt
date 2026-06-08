# libs/io/CMakeLists.txt
#
# File I/O: low-level + CSV / readtable + spreadsheets + workspace
# save/load + path helpers. Depends on builtin.

add_subdirectory(src)  # exports IOLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_CORELIBS_SOURCES ${IOLIB_SOURCES})
list(APPEND NUMKIT_CORELIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_CORELIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/file_io/fileio.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/paths/paths.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/text/csv.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/text/extras.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/io/workspace/saveload.hpp
)

set(NUMKIT_CORELIBS_SOURCES      ${NUMKIT_CORELIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_CORELIBS_HEADERS      ${NUMKIT_CORELIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_CORELIBS_INCLUDE_DIRS ${NUMKIT_CORELIBS_INCLUDE_DIRS} PARENT_SCOPE)
