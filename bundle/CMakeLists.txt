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
)
set(NUMKIT_BUNDLE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(NUMKIT_BUNDLE_SOURCES      ${NUMKIT_BUNDLE_SOURCES}      PARENT_SCOPE)
set(NUMKIT_BUNDLE_INCLUDE_DIRS ${NUMKIT_BUNDLE_INCLUDE_DIRS} PARENT_SCOPE)
