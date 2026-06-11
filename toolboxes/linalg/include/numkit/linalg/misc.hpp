// toolboxes/linalg/include/numkit/linalg/misc.hpp
//
// Miscellaneous linalg utilities: reduced row echelon form (rref),
// Givens plane rotation (planerot). Migrated from
// toolboxes/builtin/src/language/arrays/linalg_extras.cpp.

#pragma once

#include <memory_resource>
#include <utility>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Reduced row echelon form (`[R, jb] = rref(A, have_tol, tol)`).
///
/// `jb` is the 1-based pivot-column indices. Real-only in v1.
std::pair<Value, Value>
rref(const Value &A, bool have_tol, double tol_user,
     std::pmr::memory_resource *mr = nullptr);

/// @brief Givens plane rotation (`[G, y_out] = planerot([x; y])`).
///
/// Returns `G` such that `G · [x; y] = [r; 0]` where `r = hypot(x, y)`.
/// Real-only.
std::pair<Value, Value>
planerot(const Value &xy, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
