# toolboxes/control/CMakeLists.txt
#
# Control System Toolbox. Depends on builtin + signal (poly conv).

add_subdirectory(src)  # exports CONTROLLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${CONTROLLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/analyze/analyze.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/connect/connect.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/conversion/conversion.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/discretize/discretize.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/freq/freq.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/internal/numerics.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/lti/lti.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/lyapunov/lyapunov.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/props/props.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/response/response.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/state/place.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/control/state/state.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
