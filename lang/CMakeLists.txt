# lang/CMakeLists.txt
#
# numkit `lang` library (L2): MATLAB Language Fundamentals compute — arrays
# (matrix / manip / nd_manip), strings (strings/regex/format/print/scan), types
# (+ casts SIMD), bitwise, operators (binary/unary). Extracted from the dissolved
# `builtin` toolbox as step C2 of the Phase-3-A layering refactor
# (dev-docs/LAYERING_TARGET_ARCHITECTURE.md §7-C). Mirrors math/CMakeLists.txt.
#
# Compute is core-free (value / ops / fs). The engine-coupled `*_reg` adapters
# travel with it for now and move to bundle in step C5/F. The namespace rename
# numkit::builtin -> numkit::lang is sequenced into step C4 (a single mechanical
# pass over the relocated dirs); C2 only relocates files + headers so each commit
# stays trivially green.
#
# Core-COUPLED language areas (cells, structures, datatypes/containers,
# arrays/accum, commands/env) stay in builtin — they move to runtime in step C3.
#
# Per-layer link-target split is DEFERRED (step G); these sources compile INTO
# the single `numkit` target for now, exactly like math/ and toolboxes/*. This
# file only collects sources / headers / include dirs and propagates upward.

list(APPEND NUMKIT_LANG_SOURCES
    # bitwise/ (bitand/bitor/bitxor/bitshift/bitcmp/... — C2 pilot)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bitwise/int_math.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bitwise/int_math_reg.cpp
    # strings/ (string ops / regexp / sprintf-family / disp-print / sscanf-family)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/strings_reg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/regex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/regex_reg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/format_reg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/print.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/print_reg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/scan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings/scan_reg.cpp
)
list(APPEND NUMKIT_LANG_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND NUMKIT_LANG_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/lang/bitwise/int_math.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/lang/strings/strings.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/lang/strings/regex.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/lang/strings/format.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/lang/strings/print.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/lang/strings/scan.hpp
)

set(NUMKIT_LANG_SOURCES      ${NUMKIT_LANG_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LANG_HEADERS      ${NUMKIT_LANG_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LANG_INCLUDE_DIRS ${NUMKIT_LANG_INCLUDE_DIRS} PARENT_SCOPE)
