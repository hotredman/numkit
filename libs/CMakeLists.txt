# libs/CMakeLists.txt
#
# Aggregates all toolbox libraries. Each lib has its own CMakeLists.txt
# at libs/<name>/ that exports its sources/headers/include via
# PARENT_SCOPE; this file just iterates and propagates upward.
#
# To add a new lib: create libs/<name>/{include,src,tests,benchmarks}/
# with a libs/<name>/CMakeLists.txt and append <name> to the loop below.

foreach(_lib builtin signal stats image comm control wavelet graphics io optim)
    add_subdirectory(${_lib})
endforeach()

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
