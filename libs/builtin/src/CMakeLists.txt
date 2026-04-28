set(STDLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdMath.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdStats.cpp
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
)

# Backend-split functions: one definition per TU, picked at configure
# time via NUMKIT_WITH_SIMD. Keep these lines 1-per-function so the
# diff is readable when a new function gets a SIMD backend.
if(NUMKIT_WITH_SIMD)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdAbs_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdTranscendental_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdBinaryOps_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdLogicalReductions_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdCompare_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdNanReductions_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdVarReduction_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdCumSum_simd.cpp)
else()
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdAbs_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdTranscendental_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdBinaryOps_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdLogicalReductions_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdCompare_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdNanReductions_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdVarReduction_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdCumSum_portable.cpp)
endif()

set(STDLIB_SOURCES ${STDLIB_SOURCES} PARENT_SCOPE)
