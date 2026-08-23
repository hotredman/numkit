# bundle/CMakeLists.txt
#
# The composition layer: installStandardLibrary() + makeStandardEngine() +
# StandardEngine. Depends on core (Engine) and every toolbox (their install()).
#
# For now its single TU is compiled into the `numkit` aggregate (which already
# contains core + libs), and its public headers are published on the aggregate.
# When the monolith is split into per-layer targets (Phase 2) this becomes its
# own numkit_bundle library linking core + the toolboxes.

set(NUMKIT_BUNDLE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/standard_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/help/help_catalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/help/help.cpp
)
# C5: per-domain registration adapters (*_reg) relocated here from the compute
# libraries — registration glue (CallContext/Engine) belongs to the bundle layer.
# Globbed so each domain's adapters are picked up as they are relocated.
file(GLOB_RECURSE NUMKIT_BUNDLE_REG CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/register/*.cpp)
list(APPEND NUMKIT_BUNDLE_SOURCES ${NUMKIT_BUNDLE_REG})
set(NUMKIT_BUNDLE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(NUMKIT_BUNDLE_SOURCES      ${NUMKIT_BUNDLE_SOURCES}      PARENT_SCOPE)
set(NUMKIT_BUNDLE_INCLUDE_DIRS ${NUMKIT_BUNDLE_INCLUDE_DIRS} PARENT_SCOPE)
