set(STDLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    # language/ — MATLAB Language Fundamentals
    ${CMAKE_CURRENT_SOURCE_DIR}/language/commands/env.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/binary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/unary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/matrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/manip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/nd_manip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/arrays/accum.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/types/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/regex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/print.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/strings/scan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/cells/cell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/structures/struct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/language/bitwise/int_math.cpp
    # math/ — MATLAB Mathematics
    ${CMAKE_CURRENT_SOURCE_DIR}/math/random/rng.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/math/optim/fzero.cpp
    # programming/ — MATLAB Programming and Scripts
    ${CMAKE_CURRENT_SOURCE_DIR}/programming/errors/diagnostics.cpp
)

# Backend-split functions: one definition per TU, picked at configure
# time via NUMKIT_WITH_SIMD. The math backends share helpers and live
# under math/_backends/. The operator backends are co-located next to
# their consumer in language/operators/backends/.
if(NUMKIT_WITH_SIMD)
    # math/_backends/ kernels
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/abs_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/transcendental_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/logical_reductions_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/var_reduction_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/cumsum_simd.cpp)
    # language/operators/backends/ kernels
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/backends/binary_ops_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/backends/compare_simd.cpp)
else()
    # math/_backends/ kernels
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/abs_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/transcendental_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/logical_reductions_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/var_reduction_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/_backends/cumsum_portable.cpp)
    # language/operators/backends/ kernels
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/backends/binary_ops_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/language/operators/backends/compare_portable.cpp)
endif()

set(STDLIB_SOURCES ${STDLIB_SOURCES} PARENT_SCOPE)
