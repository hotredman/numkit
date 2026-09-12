# toolboxes/optim/CMakeLists.txt
#
# Optimization: fzero, fminbnd, fminsearch (constrained / global solvers
# landing). Depends on builtin.

add_subdirectory(src)  # exports OPTIMLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${OPTIMLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/optim/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/optim/local/fzero.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
