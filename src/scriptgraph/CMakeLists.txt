# scriptgraph/CMakeLists.txt
#
# Script-graph visualizer support. Pure C++ analysis pass that lowers
# a parsed AST to a NodeGraph IR (statement-level data-flow graph)
# and serializes it to JSON. Consumed by the IDE via WASM
# (`buildScriptGraph(text)`) — NOT registered as an engine builtin
# (no script-level access; this is an offline analysis tool).

add_subdirectory(src)  # exports SCRIPTGRAPHLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${SCRIPTGRAPHLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/scriptgraph/node_graph.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/scriptgraph/lowering.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/scriptgraph/serialize.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/scriptgraph/ast_serialize.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
