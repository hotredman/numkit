# toolboxes/optim/CMakeLists.txt
#
# Optimization: fzero, fminbnd, fminsearch (constrained / global solvers
# landing). Depends on builtin.

add_subdirectory(src)  # exports OPTIMLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_CORELIBS_SOURCES ${OPTIMLIB_SOURCES})
list(APPEND NUMKIT_CORELIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_CORELIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/optim/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/optim/local/fzero.hpp
)

set(NUMKIT_CORELIBS_SOURCES      ${NUMKIT_CORELIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_CORELIBS_HEADERS      ${NUMKIT_CORELIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_CORELIBS_INCLUDE_DIRS ${NUMKIT_CORELIBS_INCLUDE_DIRS} PARENT_SCOPE)
