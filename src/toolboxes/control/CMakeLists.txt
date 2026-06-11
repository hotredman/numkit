# toolboxes/control/CMakeLists.txt
#
# Control System Toolbox. Depends on builtin + signal (poly conv).

add_subdirectory(src)  # exports CONTROLLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${CONTROLLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/analyze/analyze.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/connect/connect.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/conversion/conversion.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/discretize/discretize.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/freq/freq.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/internal/numerics.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/lti/lti.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/lyapunov/lyapunov.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/props/props.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/response/response.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/state/place.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/control/state/state.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
