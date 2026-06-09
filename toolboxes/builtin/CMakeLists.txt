# toolboxes/builtin/CMakeLists.txt
#
# Base layer: language fundamentals + math + programming. Self-contained;
# no toolbox-lib dependency. Owned by the LIBS-builtin worker.

add_subdirectory(src)  # exports STDLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${STDLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/library.hpp
    # (la_solve internal kernel moved to ops/ — numkit::ops::la_solve.)
    # math/
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/math/integration/integration.hpp
    # language/
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/arrays/accum.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/arrays/manip.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/arrays/matrix.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/arrays/nd_manip.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/cells/cell.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/commands/env.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/operators/binary_ops.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/operators/unary_ops.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/strings/format.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/strings/print.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/strings/regex.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/strings/scan.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/strings/strings.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/structures/struct.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/language/types/types.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/containers.hpp
    # programming/
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/programming/errors/diagnostics.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
