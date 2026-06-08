# core-libs/CMakeLists.txt
#
# Engine-coupled support libraries (L2): callback solvers + workspace I/O that
# fundamentally need the Engine (RHS/objective callbacks, workspace save/load),
# unlike the pure-compute toolboxes/ in libs/. Same source-aggregation pattern;
# each sub-lib exports into NUMKIT_CORELIBS_* via PARENT_SCOPE.

foreach(_lib io optim ode)
    add_subdirectory(${_lib})
endforeach()

set(NUMKIT_CORELIBS_SOURCES      ${NUMKIT_CORELIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_CORELIBS_HEADERS      ${NUMKIT_CORELIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_CORELIBS_INCLUDE_DIRS ${NUMKIT_CORELIBS_INCLUDE_DIRS} PARENT_SCOPE)
