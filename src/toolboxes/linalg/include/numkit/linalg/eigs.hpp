/// @file eigs.hpp
/// @ingroup group_matfun
// toolboxes/linalg/include/numkit/linalg/eigs.hpp
//
// eigs & svds: Subset of eigenvalues / singular values via Arnoldi iteration.

#pragma once

#include <memory_resource>
#include <tuple>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Compute top k eigenvalues of A as a column vector.
Value eigs_values(const Value &A, std::size_t k = 6, std::pmr::memory_resource *mr = nullptr);

/// @brief Compute top k eigenvalues and eigenvectors of A: [V, D] = eigs(A, k).
std::tuple<Value, Value> eigs(const Value &A, std::size_t k = 6, std::pmr::memory_resource *mr = nullptr);

/// @brief Compute top k singular values of A as a column vector.
Value svds_values(const Value &A, std::size_t k = 6, std::pmr::memory_resource *mr = nullptr);

/// @brief Compute top k singular values and vectors of A: [U, S, V] = svds(A, k).
std::tuple<Value, Value, Value> svds(const Value &A, std::size_t k = 6, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
