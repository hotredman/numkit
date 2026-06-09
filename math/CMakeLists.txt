# math/CMakeLists.txt
#
# numkit `math` library (L2): MATLAB Mathematics compute — arithmetic / trig /
# exp_log / special / poly / complex / discrete / geom / interp / permutations /
# random. Extracted from the dissolved `builtin` toolbox as step C of the
# Phase-3-A layering refactor (dev-docs/LAYERING_TARGET_ARCHITECTURE.md §7-C).
#
# Compute is core-free (value / ops / fs). The engine-coupled `*_reg` adapters
# travel with it for now and move to bundle in step C5/F. The namespace rename
# numkit::builtin -> numkit::math is sequenced into step C4 (a single mechanical
# pass over the relocated dirs); C1 only relocates files + headers so each
# commit stays trivially green.
#
# Per-layer link-target split is DEFERRED (step G — cross-preset + WASM
# -fexceptions risk), so these sources compile INTO the single `numkit` target
# for now, exactly like toolboxes/* and runtime/. This file only collects
# sources / headers / include dirs and propagates them upward via PARENT_SCOPE.

list(APPEND NUMKIT_MATH_SOURCES
    # geom/ (C1 pilot — inpolygon / convhull)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/geom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/geom_reg.cpp
    # complex/ (abs/angle/conj/real/imag/complex/iscomplex/...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/complex/complex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/complex/complex_reg.cpp
    # discrete/ (unique/ismember/union/intersect/setdiff/primes/factor/perms/histc...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/discrete/discrete.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/discrete/discrete_reg.cpp
)
list(APPEND NUMKIT_MATH_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND NUMKIT_MATH_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/geom/geom.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/complex/complex.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/discrete/discrete.hpp
)

set(NUMKIT_MATH_SOURCES      ${NUMKIT_MATH_SOURCES}      PARENT_SCOPE)
set(NUMKIT_MATH_HEADERS      ${NUMKIT_MATH_HEADERS}      PARENT_SCOPE)
set(NUMKIT_MATH_INCLUDE_DIRS ${NUMKIT_MATH_INCLUDE_DIRS} PARENT_SCOPE)
