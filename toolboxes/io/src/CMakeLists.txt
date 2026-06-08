set(IOLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_io/fileio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text/csv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text/extras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/paths/paths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workspace/saveload.cpp
)
# Binary .mat backend pulls in matio.h directly — only compile it when
# the matio dep is linked (NUMKIT_WITH_MATIO=ON).
if(NUMKIT_WITH_MATIO)
    list(APPEND IOLIB_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/workspace/saveload_mat.cpp)
endif()
set(IOLIB_SOURCES ${IOLIB_SOURCES} PARENT_SCOPE)
