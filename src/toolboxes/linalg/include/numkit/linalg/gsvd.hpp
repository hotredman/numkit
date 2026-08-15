// toolboxes/linalg/include/numkit/linalg/gsvd.hpp
//
// Generalized Singular Value Decomposition: [U, V, X, C, S] = gsvd(A, B) or sigma = gsvd(A, B)

#pragma once

#include <memory_resource>
#include <tuple>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Compute generalized singular values of matrix pair (A, B) as a column vector (ascending order).
Value gsvd_values(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// @brief Full generalized SVD decomposition of matrix pair (A, B).
/// Returns std::tuple<Value, Value, Value, Value, Value>{U, V, X, C, S}
/// such that A = U * C * X^H and B = V * S * X^H.
std::tuple<Value, Value, Value, Value, Value> gsvd(const Value &A, const Value &B,
                                                   std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
