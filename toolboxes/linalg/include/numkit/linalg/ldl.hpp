// toolboxes/linalg/include/numkit/linalg/ldl.hpp
//
// MATLAB ldl: block LDL' factorization. Migrated from
// toolboxes/builtin/src/language/arrays/ldl.cpp.

#pragma once

#include <memory_resource>
#include <tuple>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Block LDL' factorisation (`[L, D, P] = ldl(A, upper_form, p_as_vector)`).
///
/// v1 implements Crout LDL' without pivoting (works for PD/ND and most
/// indefinite matrices). `P` is identity in v1. Bunch-Kaufman 2×2
/// pivoting and the sparse `[L, D, P, C]` form are deferred.
///
/// @param A             Symmetric matrix.
/// @param upper_form    When `true`, return upper-triangular `L`.
/// @param p_as_vector   When `true`, return `P` as a `1 × n` perm vector.
/// @param mr            Memory resource (nullptr → process default).
/// @return              `(L, D, P)` triple.
std::tuple<Value, Value, Value>
ldl(const Value &A, bool upper_form, bool p_as_vector,
    std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
