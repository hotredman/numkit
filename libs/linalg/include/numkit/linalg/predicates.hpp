// libs/linalg/include/numkit/linalg/predicates.hpp
//
// Matrix-structure predicates and bandwidth queries. Migrated from
// libs/builtin/src/language/arrays/predicates.cpp.

#pragma once

#include <memory_resource>
#include <string>
#include <utility>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Banded structure test (`tf = isbanded(A, lower, upper)`).
///
/// True iff entries outside the `[-lower, +upper]` diagonal band are
/// exactly zero. Exact comparison (no tolerance).
Value isbanded(const Value &A, long lower, long upper, std::pmr::memory_resource *mr = nullptr);

/// @brief Diagonal structure (`tf = isdiag(A)`). True iff `isbanded(A, 0, 0)`.
Value isdiag(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Lower-triangular structure (`tf = istril(A)`).
Value istril(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Upper-triangular structure (`tf = istriu(A)`).
Value istriu(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Symmetry test (`tf = issymmetric(A, skew)`).
///
/// `skew == false` (default): `A == A.'` (transpose, no conj).
/// `skew == true`: `A == -A.'`.
Value issymmetric(const Value &A, bool skew = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Hermitian test (`tf = ishermitian(A, skew)`).
Value ishermitian(const Value &A, bool skew = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Bandwidth pair (`[lower, upper] = bandwidth(A)`).
std::pair<Value, Value>
bandwidth(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Single-output bandwidth selector (`x = bandwidth(A, which)`).
Value bandwidthOpt(const Value &A, const std::string &which, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
