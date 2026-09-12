# scriptgraph/CMakeLists.txt
#
# Script-graph visualizer support. Pure C++ analysis pass that lowers
# a parsed AST to a NodeGraph IR (statement-level data-flow graph)
# and serializes it to JSON. Consumed by the IDE via WASM
# (`buildScriptGraph(text)`) — NOT registered as an engine builtin
# (no script-level access; this is an offline analysis tool).

add_subdirectory(src)  # exports SCRIPTGRAPHLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${SCRIPTGRAPHLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/scriptgraph/node_graph.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/scriptgraph/lowering.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/scriptgraph/serialize.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/scriptgraph/ast_serialize.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
