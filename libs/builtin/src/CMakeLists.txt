set(STDLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    # lang/
    ${CMAKE_CURRENT_SOURCE_DIR}/lang/commands/env.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lang/operators/binary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lang/operators/unary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lang/arrays/matrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lang/arrays/manip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lang/arrays/nd_manip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lang/arrays/accum.cpp
    # math/
    ${CMAKE_CURRENT_SOURCE_DIR}/math/random/rng.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/complex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/polynomials.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/discrete.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/trigonometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/exponents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/rounding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/misc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/special.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/reductions.cpp
    # math/interpolation/ — merged from libs/fit
    ${CMAKE_CURRENT_SOURCE_DIR}/math/interpolation/interp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/integration/integration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math/optim/fzero.cpp
    # data_io/
    ${CMAKE_CURRENT_SOURCE_DIR}/data_io/csv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/data_io/fileio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/data_io/saveload.cpp
    # datatypes/
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes/numeric/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes/numeric/int_math.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes/strings/strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes/strings/regex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes/strings/format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes/strings/print.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes/strings/scan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes/cell/cell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes/struct/struct.cpp
    # programming/
    ${CMAKE_CURRENT_SOURCE_DIR}/programming/errors/diagnostics.cpp
    # data_analysis/
    ${CMAKE_CURRENT_SOURCE_DIR}/data_analysis/descriptive_statistics/stats.cpp
)

# Backend-split functions: one definition per TU, picked at configure
# time via NUMKIT_WITH_SIMD. Co-located in backends/ next to their
# public-API consumer folder. Keep these lines 1-per-function so the
# diff is readable when a new function gets a SIMD backend.
if(NUMKIT_WITH_SIMD)
    # math/elementary kernels
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/abs_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/transcendental_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/logical_reductions_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/var_reduction_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/cumsum_simd.cpp)
    # lang/operators kernels
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lang/operators/backends/binary_ops_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lang/operators/backends/compare_simd.cpp)
else()
    # math/elementary kernels
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/abs_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/transcendental_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/logical_reductions_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/var_reduction_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/math/elementary/backends/cumsum_portable.cpp)
    # lang/operators kernels
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lang/operators/backends/binary_ops_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lang/operators/backends/compare_portable.cpp)
endif()

set(STDLIB_SOURCES ${STDLIB_SOURCES} PARENT_SCOPE)
