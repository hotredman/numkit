set(IOLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_io/fileio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text/csv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text/extras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/paths/paths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workspace/saveload.cpp
)
set(IOLIB_SOURCES ${IOLIB_SOURCES} PARENT_SCOPE)
