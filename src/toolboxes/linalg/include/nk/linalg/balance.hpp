/// @file balance.hpp
/// @ingroup group_matfun
// toolboxes/linalg/include/nk/linalg/balance.hpp
//
// MATLAB balance: diagonal-similarity scaling for eigenvalue
// computations. Migrated from toolboxes/builtin/src/language/arrays/balance.cpp.

#pragma once

#include <memory_resource>
#include <nk/value/value.hpp>

namespace nk::linalg {

/// @addtogroup group_matfun
/// @{


/// @brief Result of @ref balance_impl.
struct BalanceResult {
    Value B;         ///< Balanced matrix.
    Value d_col;     ///< Column of scaling factors.
    Value perm_col;  ///< Column of permutation indices (1..n in v1).
};

/// @brief Diagonal-similarity balancing for eigenvalue computations
/// (`[B, d, p] = balance(A, noperm)`).
///
/// Parlett-Reinsch (1969). v1 implements only the scaling phase
/// (permutation phase deferred; behaves like `balance(A, 'noperm')`).
BalanceResult balance_impl(const Value &A, bool noperm,
                           std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace nk::linalg
