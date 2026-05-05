set(OPTIMLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    # local/ — MATLAB-base optimization (Mathematics > Optimization)
    ${CMAKE_CURRENT_SOURCE_DIR}/local/fzero.cpp
)

set(OPTIMLIB_SOURCES ${OPTIMLIB_SOURCES} PARENT_SCOPE)
