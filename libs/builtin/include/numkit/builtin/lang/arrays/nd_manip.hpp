// libs/builtin/include/numkit/builtin/lang/arrays/nd_manip.hpp
//
// Phase-6 N-D array manipulation: permute / ipermute / squeeze /
// cat(dim, ...) / blkdiag. numkit-m's Value currently caps at 3D,
// so all permutation vectors must have length 2 (matrix) or 3 (3D).
// Higher-dim is not representable and will throw.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <cstddef>

namespace numkit::builtin {

/// permute(A, perm) — perm is a 1-based permutation of [1..ndims(A)].
/// `B = permute(A, [2 1])` for a matrix is the transpose. Pointer + size
/// so the same overload composes with std::vector / pmr::vector / arrays.
Value permute(std::pmr::memory_resource *mr, const Value &x, const int *perm, std::size_t n);

/// ipermute — inverse of permute.
/// `ipermute(permute(A, p), p) == A` for any valid p.
Value ipermute(std::pmr::memory_resource *mr, const Value &x, const int *perm, std::size_t n);

/// squeeze — drop singleton dimensions. Vectors and 2D matrices are
/// returned unchanged (MATLAB doesn't squeeze 2D below 2D); 3D arrays
/// with at least one singleton dim collapse down to 2D.
Value squeeze(std::pmr::memory_resource *mr, const Value &x);

/// cat(dim, A, B, ...) — concatenate along dim 1, 2, or 3.
/// dim=1 == vertcat, dim=2 == horzcat, dim=3 stacks 2D pages into 3D
/// (or extends an existing 3D's page count). All inputs must agree on
/// the non-`dim` dimensions.
Value cat(std::pmr::memory_resource *mr, int dim, const Value *values, size_t count);

/// blkdiag(A, B, C, ...) — block-diagonal matrix with the inputs on
/// the diagonal and zeros elsewhere. 2D inputs only.
Value blkdiag(std::pmr::memory_resource *mr, const Value *values, size_t count);

} // namespace numkit::builtin
