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
    # la_solve.cpp moved to ops/ (numkit_ops) — shared linear-solve kernel
    # used by builtin `\` and libs/linalg; ops/ sits below both.
    ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/unary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/matrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/manip.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/manip_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/nd_manip.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/nd_manip_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/accum.cpp
    # predicates.cpp / linalg_extras.cpp / ldl.cpp / balance.cpp /
    # lsq.cpp all migrated to libs/linalg.
    ${CMAKE_CURRENT_SOURCE_DIR}/language/types/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/regex.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/regex_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/format.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/format_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/print.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/print_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/scan.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/scan_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/cells/cell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/structures/struct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/datatypes/containers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/bitwise/int_math.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/language/bitwise/int_math_reg.cpp # register (CallContext)
    # math/ — MATLAB Mathematics
    ${CMAKE_CURRENT_SOURCE_DIR}/math/random/rng.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/random/rng_reg.cpp # register (CallContext)
    # matlab_mt19937.cpp moved to ops/ (numkit_ops) — the MT19937 engine is a
    # shared primitive; builtin's rng.cpp uses it via numkit::ops.
    ${CMAKE_CURRENT_SOURCE_DIR}/math/complex/complex.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/complex/complex_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/poly/polynomials.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/discrete/discrete.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/discrete/discrete_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trigonometry.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trigonometry_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/exp_log/exponents.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/exp_log/exponents_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/special/special.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/reductions.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/reductions_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/rounding.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/rounding_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/misc.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/misc_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/interp/interp.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/interp/interp_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/integration/integration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/geom/geom.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/geom/geom_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/group/group.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/permutations.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/math/permutations_reg.cpp # register (CallContext)
    # math/optim/ moved to libs/optim/
    # programming/ — MATLAB Programming and Scripts
    ${CMAKE_CURRENT_SOURCE_DIR}/programming/errors/diagnostics.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/programming/errors/diagnostics_reg.cpp # register (CallContext)
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
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/mod_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trig_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trig_recip_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/exp_log/exp_log_highway.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/special/special_highway.cpp)
    # (binary_ops_highway.cpp + compare_highway.cpp moved to ops/ → numkit_ops)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/types/casts_highway.cpp)
else()
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/abs_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/cumsum_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/logical_reductions_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/var_reduction_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/isfinite_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/rounding_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/arithmetic/mod_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trig_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/trig/trig_recip_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/exp_log/exp_log_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/special/special_portable.cpp)
    # (binary_ops_portable.cpp + compare_portable.cpp moved to ops/ → numkit_ops)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/types/casts_portable.cpp)
endif()

set(STDLIB_SOURCES ${STDLIB_SOURCES} PARENT_SCOPE)
