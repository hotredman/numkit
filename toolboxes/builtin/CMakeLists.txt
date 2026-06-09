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
    # math/ — all relocated (integration.hpp -> runtime in C3; the rest -> math lib in C1)
    # language/ — all relocated (cell/struct -> runtime in C3; the rest -> lang lib in C2)
    # programming/
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/programming/errors/diagnostics.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
