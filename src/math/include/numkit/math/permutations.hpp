// toolboxes/builtin/include/numkit/builtin/math/permutations.hpp
//
// Sparse-style permutation utilities (colperm, symrcm) for dense matrices.
// MATLAB documents these for sparse matrices, but they operate on dense
// matrices too (any element != 0 counts as a nonzero).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::math {

/// @brief Column permutation by ascending nonzero count (`j = colperm(S)`).
///
/// Returns a 1-based row-vector permutation that orders the columns of `S`
/// by ascending count of nonzero entries (stable — ties keep the original
/// column order).
///
/// @param S   2-D matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row vector of 1-based column indices.
/// @throws Error if `S` is not a 2-D matrix.
/// @see symrcm
Value colperm(const Value &S, std::pmr::memory_resource *mr = nullptr);

/// @brief Symmetric reverse Cuthill-McKee ordering (`p = symrcm(S)`).
///
/// Bandwidth-reducing symmetric permutation of the square matrix `S`. Builds
/// the undirected adjacency pattern from `|S| + |S^T|` (off-diagonal), then
/// per connected component runs a degree-ordered BFS and reverses it
/// (Cuthill-McKee 1969 / George 1971).
///
/// @param S   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row vector of 1-based reordering indices.
/// @throws Error if `S` is not square.
/// @see colperm
Value symrcm(const Value &S, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::math
