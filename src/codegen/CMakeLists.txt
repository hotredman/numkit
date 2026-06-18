# codegen/CMakeLists.txt
#
# numkit code generation — a static type-inference pass over a parsed
# AST (AST -> typed IR) and a transpiler that emits C++ calling the
# existing numkit runtime, for AOT compilation of numkit code via an
# external compiler.
#
# Pure C++ analysis/codegen pass: core-coupled (parser / AST), registers
# NO engine builtin. Architectural sibling of scriptgraph (L2 analysis
# layer). Compiled into numkit_toolboxes_obj via NUMKIT_LIBS_SOURCES.

add_subdirectory(src)  # exports CODEGENLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${CODEGENLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/codegen/type_lattice.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
