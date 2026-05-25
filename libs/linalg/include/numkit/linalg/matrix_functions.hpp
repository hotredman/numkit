// libs/linalg/include/numkit/linalg/matrix_functions.hpp
//
// Matrix functions: expm, logm, sqrtm. Migrated from
// libs/builtin/src/language/arrays/matrix.cpp.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::linalg {

/// @brief Matrix exponential (`B = expm(A)`).
///
/// Padé(6) approximation with scaling-and-squaring (Higham 2005).
Value expm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix logarithm for symmetric positive-definite A.
///
/// Via eigendecomposition: `logm(A) = V · diag(log(eig)) · V'`.
/// General logm requires complex Schur (deferred).
Value logm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix square root for symmetric PSD A.
Value sqrtm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
