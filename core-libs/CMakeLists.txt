# core-libs/CMakeLists.txt
#
# Language-runtime layer (L2, engine-coupled): the scripting runtime itself
# (eval-family today; workspace who/clear/import clusters land here as the
# extraction out of toolboxes/builtin proceeds). NOT a math/io toolbox.
#
# Per-layer link-target split is DEFERRED (see Phase 3-A — cross-preset + WASM
# -fexceptions risk), so these sources are compiled INTO the single `numkit`
# target for now, exactly like the toolboxes. This file only collects sources /
# headers / include dirs and propagates them upward via PARENT_SCOPE, mirroring
# toolboxes/<name>/CMakeLists.txt.

list(APPEND NUMKIT_CORELIBS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/runtime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/eval.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/workspace.cpp
)
list(APPEND NUMKIT_CORELIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND NUMKIT_CORELIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/corelibs/runtime.hpp
)

set(NUMKIT_CORELIBS_SOURCES      ${NUMKIT_CORELIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_CORELIBS_HEADERS      ${NUMKIT_CORELIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_CORELIBS_INCLUDE_DIRS ${NUMKIT_CORELIBS_INCLUDE_DIRS} PARENT_SCOPE)
