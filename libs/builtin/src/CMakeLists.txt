set(STDLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    # Tiny isolated TU owning the build-timestamp string. Forced to
    # recompile on every build by the `numkit_build_info` target in
    # the top-level CMakeLists, so the `version` builtin always
    # reflects the actual link time instead of stale __DATE__.
    ${CMAKE_CURRENT_SOURCE_DIR}/version_string.cpp
    # language/ — MATLAB Language Fundamentals
    ${CMAKE_CURRENT_SOURCE_DIR}/language/commands/env.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/binary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/la_solve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/unary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/matrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/manip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/nd_manip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/accum.cpp
    # predicates.cpp / linalg_extras.cpp / ldl.cpp / balance.cpp /
    # lsq.cpp all migrated to libs/linalg.
    ${CMAKE_CURRENT_SOURCE_DIR}/language/types/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/regex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/print.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/scan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/cells/cell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/structures/struct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/datatypes/containers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/bitwise/int_math.cpp
    # math/ — MATLAB Mathematics
    ${CMAKE_CURRENT_SOURCE_DIR}/math/random/rng.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/random/matlab_mt19937.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/complex/complex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/poly/polynomials.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/discrete/discrete.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trigonometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/exp_log/exponents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/special/special.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/reductions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/rounding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/misc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/interp/interp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/integration/integration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/geom/geom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/group/group.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/permutations.cpp
    # math/optim/ moved to libs/optim/
    # programming/ — MATLAB Programming and Scripts
    ${CMAKE_CURRENT_SOURCE_DIR}/programming/errors/diagnostics.cpp
)

# Backend-split functions: one definition per TU, picked at configure
# time via NUMKIT_WITH_SIMD. SIMD and portable variants live next to
# their public-API consumers in the same feature directory:
#   math/arithmetic/  — abs, cumsum, logical reductions, var reduction
#   math/trig/        — sin, cos (trig family)
#   math/exp_log/     — exp, log (exp/log family)
#   language/operators/backends/ — binary ops, compare
if(NUMKIT_WITH_SIMD)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/abs_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/cumsum_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/logical_reductions_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/var_reduction_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/isfinite_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/rounding_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trig_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trig_recip_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/exp_log/exp_log_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/backends/binary_ops_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/backends/compare_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/types/casts_highway.cpp)
else()
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/abs_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/cumsum_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/logical_reductions_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/var_reduction_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/isfinite_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/rounding_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trig_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trig_recip_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/exp_log/exp_log_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/backends/binary_ops_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/backends/compare_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/types/casts_portable.cpp)
endif()

set(STDLIB_SOURCES ${STDLIB_SOURCES} PARENT_SCOPE)
