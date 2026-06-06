# core/CMakeLists.txt
#
# Engine + frontend + backend + debugger. Self-contained: no toolbox-lib
# dependency. Owned by the CORE worker per dev-docs/COORDINATION.md.
#
# Exports via PARENT_SCOPE for the top-level CMakeLists.txt to assemble:
#   NUMKIT_CORE_SOURCES      — .cpp source list
#   NUMKIT_CORE_HEADERS      — public .hpp list (kept for IDE source-group)
#   NUMKIT_CORE_INCLUDE_DIRS — public include path

set(NUMKIT_CORE_SOURCES
    # (Value substrate — value_type/dims/data_buffer/heap_object/value.cpp —
    #  moved to value/ → numkit_value L0 module)
    # (vfs.cpp moved to fs/ → numkit_fs L0 module)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/environment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine.cpp
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

# (thread_pool.cpp + parallel_for.hpp/thread_pool.hpp moved to ops/ → numkit_ops)

set(NUMKIT_CORE_HEADERS
    # (Value substrate headers — value/span/heap_object/data_buffer/dims/
    #  value_type/object/shape_ops/fn_handle/function_ref/scratch — moved to
    #  value/ → numkit_value L0 module)
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/ast.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/branding.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/bytecode.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/compiler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/debugger.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/debug_session.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/debug_workspace.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/engine.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/environment.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/figure_manager.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/lexer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/parser.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/tree_walker.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/types.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/core/vm.hpp
)

set(NUMKIT_CORE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(NUMKIT_CORE_SOURCES      ${NUMKIT_CORE_SOURCES}      PARENT_SCOPE)
set(NUMKIT_CORE_HEADERS      ${NUMKIT_CORE_HEADERS}      PARENT_SCOPE)
set(NUMKIT_CORE_INCLUDE_DIRS ${NUMKIT_CORE_INCLUDE_DIRS} PARENT_SCOPE)
