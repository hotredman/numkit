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
    # complex/ (abs/angle/conj/real/imag/complex/iscomplex/...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/complex/complex.cpp
    # discrete/ (unique/ismember/union/intersect/setdiff/primes/factor/perms/histc...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/discrete/discrete.cpp
    # interp/ (interp1 / interp2 / spline / pchip / ...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interp/interp.cpp
    # poly/ (roots / polyval / polyfit / conv / deconv / ...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/poly/polynomials.cpp
    # permutations (top-level: perms / nchoosek / factorial / ...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/permutations.cpp
    # random/ (rand / randn / randi / randperm / rng — engine in ops, wrappers here)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/random/rng.cpp
    # trig/ (sin/cos/tan/asin/... compute + register; SIMD backends in the
    # NUMKIT_WITH_SIMD block below)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/trig/trigonometry.cpp
    # exp_log/ (exp/log/log2/log10/sqrt/pow/... compute + register; SIMD below)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/exp_log/exponents.cpp
    # special/ (gamma/beta/erf/factorial-ish... compute + register; SIMD below)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/special/special.cpp
    # arithmetic/ (misc/reductions/rounding compute + register; SIMD backends below)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/misc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/reductions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/rounding.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/arithmetic/misc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/arithmetic/reductions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/math/arithmetic/rounding.hpp
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
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/abs_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/cumsum_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/logical_reductions_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/var_reduction_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/isfinite_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/rounding_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/mod_highway.cpp)
else()
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/trig/trig_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/trig/trig_recip_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/exp_log/exp_log_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/special/special_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/abs_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/cumsum_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/logical_reductions_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/var_reduction_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/isfinite_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/rounding_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/mod_portable.cpp)
endif()

set(NUMKIT_MATH_SOURCES      ${NUMKIT_MATH_SOURCES}      PARENT_SCOPE)
set(NUMKIT_MATH_HEADERS      ${NUMKIT_MATH_HEADERS}      PARENT_SCOPE)
set(NUMKIT_MATH_INCLUDE_DIRS ${NUMKIT_MATH_INCLUDE_DIRS} PARENT_SCOPE)
