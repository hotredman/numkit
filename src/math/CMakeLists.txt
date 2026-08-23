# math/CMakeLists.txt
#
# numkit `math` library (L2): MATLAB Mathematics compute — remaining modules
# being migrated into numkit::builtin.

list(APPEND NUMKIT_MATH_SOURCES
    # geom/ (inpolygon / convhull)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/geom.cpp
    # discrete/ (unique/ismember/union/intersect/setdiff/primes/factor/perms/histc...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/discrete/discrete.cpp
    # interp/ (interp1 / interp2 / spline / pchip / ...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interp/interp.cpp
    # poly/ (roots / polyval / polyfit / conv / deconv / ...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/poly/polynomials.cpp
    # permutations (top-level: perms / nchoosek / factorial / ...)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/permutations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/integration/integration.cpp
    # random/ (rand / randn / randi / randperm / rng — engine in ops, wrappers here)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/random/rng.cpp
    # special/ (gamma/beta/erf/factorial-ish... compute + register; SIMD below)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/special/special.cpp
    # arithmetic/reductions
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/reductions.cpp
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

# Backend-split SIMD sources
if(NUMKIT_WITH_SIMD)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/special/special_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/cumsum_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/logical_reductions_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/var_reduction_highway.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/isfinite_highway.cpp)
else()
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/special/special_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/cumsum_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/logical_reductions_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/var_reduction_portable.cpp)
    list(APPEND NUMKIT_MATH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic/isfinite_portable.cpp)
endif()

set(NUMKIT_MATH_SOURCES      ${NUMKIT_MATH_SOURCES}      PARENT_SCOPE)
set(NUMKIT_MATH_HEADERS      ${NUMKIT_MATH_HEADERS}      PARENT_SCOPE)
set(NUMKIT_MATH_INCLUDE_DIRS ${NUMKIT_MATH_INCLUDE_DIRS} PARENT_SCOPE)
