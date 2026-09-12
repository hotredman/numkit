# bundle/CMakeLists.txt
#
# The composition layer: installStandardLibrary() + makeStandardEngine() +
# StandardEngine. Depends on core (Engine) and every toolbox (their install()).
#
# For now its single TU is compiled into the `nk` aggregate (which already
# contains core + libs), and its public headers are published on the aggregate.
# When the monolith is split into per-layer targets (Phase 2) this becomes its
# own nk_bundle library linking core + the toolboxes.

set(NK_BUNDLE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/standard_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/help/help_catalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/help/help.cpp
)
# C5: per-domain registration adapters (*_reg) relocated here from the compute
# libraries — registration glue (CallContext/Engine) belongs to the bundle layer.
# Globbed so each domain's adapters are picked up as they are relocated.
file(GLOB_RECURSE NK_BUNDLE_REG CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/register/*.cpp)
list(APPEND NK_BUNDLE_SOURCES ${NK_BUNDLE_REG})
set(NK_BUNDLE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(NK_BUNDLE_SOURCES      ${NK_BUNDLE_SOURCES}      PARENT_SCOPE)
set(NK_BUNDLE_INCLUDE_DIRS ${NK_BUNDLE_INCLUDE_DIRS} PARENT_SCOPE)
