# core/CMakeLists.txt
#
# Engine + frontend + backend + debugger. Self-contained: no toolbox-lib
# dependency. Owned by the CORE worker per dev-docs/handbook/coordination.md.
#
# Exports via PARENT_SCOPE for the top-level CMakeLists.txt to assemble:
#   NK_CORE_SOURCES      — .cpp source list
#   NK_CORE_HEADERS      — public .hpp list (kept for IDE source-group)
#   NK_CORE_INCLUDE_DIRS — public include path

set(NK_CORE_SOURCES
    # (Value substrate — value_type/dims/data_buffer/heap_object/value.cpp —
    #  moved to value/ → nk_value L0 module)
    # (vfs.cpp moved to fs/ → nk_fs L0 module)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/environment.cpp
    # NK Play (portion A): manifest extraction + statement
    # dependency analysis — pure, STL-only, engine-independent.
    ${CMAKE_CURRENT_SOURCE_DIR}/src/play_manifest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dependency_graph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine.cpp
    # lexer / parser / AST / compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stack_guard.cpp
    # execution backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tree_walker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm.cpp
    # debugger
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debugger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_workspace.cpp
)

# (thread_pool.cpp + parallel_for.hpp/thread_pool.hpp moved to ops/ → nk_ops)

set(NK_CORE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/play_manifest.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/dependency_graph.hpp
    # (Value substrate headers — value/span/heap_object/data_buffer/dims/
    #  value_type/object/shape_ops/fn_handle/function_ref/scratch — moved to
    #  value/ → nk_value L0 module)
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/ast.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/bytecode.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/compiler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/debugger.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/debug_session.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/debug_workspace.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/engine.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/environment.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/lexer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/parser.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/stack_guard.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/tree_walker.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/types.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/core/vm.hpp
)

set(NK_CORE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(NK_CORE_SOURCES      ${NK_CORE_SOURCES}      PARENT_SCOPE)
set(NK_CORE_HEADERS      ${NK_CORE_HEADERS}      PARENT_SCOPE)
set(NK_CORE_INCLUDE_DIRS ${NK_CORE_INCLUDE_DIRS} PARENT_SCOPE)
