set(IOLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_io/fileio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text/csv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text/extras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/paths/paths.cpp
)
# (workspace save/load + the matio v5 .mat backend moved to
#  runtime — they are workspace runtime, not data import/export.)
set(IOLIB_SOURCES ${IOLIB_SOURCES} PARENT_SCOPE)
