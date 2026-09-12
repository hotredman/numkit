# toolboxes/ode/CMakeLists.txt
#
# Ordinary Differential Equations Toolbox.
# Solvers: ode45, ode23, ode113, ode15s, ode23s, ode23t, ode23tb, ode78, ode89.
# Options: odeset, odeget. Output helpers: deval, decic, odejacobian.

add_subdirectory(src)  # exports ODELIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${ODELIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/ode/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/ode/solvers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/ode/options.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
