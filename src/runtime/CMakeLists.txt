# runtime/CMakeLists.txt
#
# Language-runtime layer (L2, engine-coupled): the scripting runtime itself
# (eval-family + workspace clear/import/assignin/inputname/who/whos/exist/
# clearvars + save/load workspace persistence incl. the matio v5 .mat
# backend). NOT a math/io toolbox.
#
# Per-layer link-target split is DEFERRED (see Phase 3-A — cross-preset + WASM
# -fexceptions risk), so these sources are compiled INTO the single `numkit`
# target for now, exactly like the toolboxes. This file only collects sources /
# headers / include dirs and propagates them upward via PARENT_SCOPE, mirroring
# toolboxes/<name>/CMakeLists.txt.

list(APPEND NUMKIT_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/eval.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/workspace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/function_handles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/saveload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/saveload_ascii.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/saveload_mat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/env.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/containers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/splitapply_callback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/struct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/print_io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scan_io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/diagnostics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/reflection.cpp
)
list(APPEND NUMKIT_RUNTIME_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND NUMKIT_RUNTIME_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/runtime.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/saveload.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/env.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/containers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/cell.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/struct.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/diagnostics.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/io.hpp
)

set(NUMKIT_RUNTIME_SOURCES      ${NUMKIT_RUNTIME_SOURCES}      PARENT_SCOPE)
set(NUMKIT_RUNTIME_HEADERS      ${NUMKIT_RUNTIME_HEADERS}      PARENT_SCOPE)
set(NUMKIT_RUNTIME_INCLUDE_DIRS ${NUMKIT_RUNTIME_INCLUDE_DIRS} PARENT_SCOPE)
