# core/CMakeLists.txt
#
# Engine + frontend + backend + debugger. Self-contained: no toolbox-lib
# dependency. Owned by the CORE worker per dev-docs/design/COORDINATION.md.
#
# Exports via PARENT_SCOPE for the top-level CMakeLists.txt to assemble:
#   NUMKIT_CORE_SOURCES      — .cpp source list
#   NUMKIT_CORE_HEADERS      — public .hpp list (kept for IDE source-group)
#   NUMKIT_CORE_INCLUDE_DIRS — public include path

set(NUMKIT_CORE_SOURCES
    # value / environment
    ${CMAKE_CURRENT_SOURCE_DIR}/src/value_type.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dims.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/heap_object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/environment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vfs.cpp
    # lexer / parser / AST / compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler.cpp
    # execution backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tree_walker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm.cpp
    # debugger
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debugger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_workspace.cpp
)

if(NUMKIT_WITH_THREADS)
    list(APPEND NUMKIT_CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/thread_pool.cpp)
endif()

set(NUMKIT_CORE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/ast.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/branding.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/bytecode.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/compiler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/data_buffer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/debugger.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/debug_session.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/debug_workspace.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/dims.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/engine.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/environment.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/figure_manager.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/heap_object.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/lexer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/parser.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/parallel_for.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/scratch.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/shape_ops.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/span.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/thread_pool.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/tree_walker.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/types.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/value.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/value_type.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/vfs.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/vm.hpp
)

set(NUMKIT_CORE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(NUMKIT_CORE_SOURCES      ${NUMKIT_CORE_SOURCES}      PARENT_SCOPE)
set(NUMKIT_CORE_HEADERS      ${NUMKIT_CORE_HEADERS}      PARENT_SCOPE)
set(NUMKIT_CORE_INCLUDE_DIRS ${NUMKIT_CORE_INCLUDE_DIRS} PARENT_SCOPE)
