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
    # interp/ (interp1 / interp2 / spline / pchip / ...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interp/interp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interp/interp_reg.cpp
    # poly/ (roots / polyval / polyfit / conv / deconv / ...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/poly/polynomials.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/poly/polynomials_reg.cpp
    # permutations (top-level: perms / nchoosek / factorial / ...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/permutations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/permutations_reg.cpp
    # random/ (rand / randn / randi / randperm / rng — engine in ops, wrappers here)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/random/rng.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/random/rng_reg.cpp
    # trig/ (sin/cos/tan/asin/... compute + register; SIMD backends in the
    # NUMKIT_WITH_SIMD block below)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/trig/trigonometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/trig/trigonometry_reg.cpp
    # exp_log/ (exp/log/log2/log10/sqrt/pow/... compute + register; SIMD below)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/exp_log/exponents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/exp_log/exponents_reg.cpp
    # special/ (gamma/beta/erf/factorial-ish... compute + register; SIMD below)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/special/special.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/special/special_reg.cpp
)
list(APPEND NUMKIT_MATH_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND NUMKIT_MATH_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/geom/geom.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/complex/complex.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/discrete/discrete.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/interp/interp.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/poly/polynomials.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/permutations.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/random/rng.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/trig/trigonometry.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/exp_log/exponents.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/special/special.hpp
)

# Backend-split SIMD sources — one variant picked at configure time via
# NUMKIT_WITH_SIMD (mirrors toolboxes/builtin/src/CMakeLists). Relocated from
# builtin in Phase 3-A step C1. The relocated *_highway.cpp set their
# HWY_TARGET_INCLUDE relative to math/src (on the numkit PRIVATE -I).
if(NUMKIT_WITH_SIMD)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/trig/trig_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/trig/trig_recip_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/exp_log/exp_log_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/special/special_highway.cpp)
else()
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/trig/trig_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/trig/trig_recip_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/exp_log/exp_log_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/special/special_portable.cpp)
endif()

set(NUMKIT_MATH_SOURCES      ${NUMKIT_MATH_SOURCES}      PARENT_SCOPE)
set(NUMKIT_MATH_HEADERS      ${NUMKIT_MATH_HEADERS}      PARENT_SCOPE)
set(NUMKIT_MATH_INCLUDE_DIRS ${NUMKIT_MATH_INCLUDE_DIRS} PARENT_SCOPE)
