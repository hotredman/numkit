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
    # Core-coupled builtins relocated from builtin (Phase 3-A step C3) — their
    # compute pulls core/engine|vm|callback_builtin, so they belong in runtime.
    ${CMAKE_CURRENT_SOURCE_DIR}/src/language/commands/env.cpp
)
# Binary .mat backend pulls in matio.h directly — only compile it when the
# matio dep is linked (NUMKIT_WITH_MATIO=ON). matio.h and the
# NUMKIT_WITH_MATIO define reach this TU via the umbrella `numkit` target
# (matio is linked PRIVATE there), into which these sources compile.
if(NUMKIT_WITH_MATIO)
    list(APPEND NUMKIT_RUNTIME_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/saveload_mat.cpp)
endif()
list(APPEND NUMKIT_RUNTIME_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND NUMKIT_RUNTIME_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/runtime.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/saveload.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/runtime/language/commands/env.hpp
)

set(NUMKIT_RUNTIME_SOURCES      ${NUMKIT_RUNTIME_SOURCES}      PARENT_SCOPE)
set(NUMKIT_RUNTIME_HEADERS      ${NUMKIT_RUNTIME_HEADERS}      PARENT_SCOPE)
set(NUMKIT_RUNTIME_INCLUDE_DIRS ${NUMKIT_RUNTIME_INCLUDE_DIRS} PARENT_SCOPE)
