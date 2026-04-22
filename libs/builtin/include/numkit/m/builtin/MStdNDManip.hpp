// libs/builtin/include/numkit/m/builtin/MStdNDManip.hpp
//
// Phase-6 N-D array manipulation: permute / ipermute / squeeze /
// cat(dim, ...) / blkdiag. numkit-m's MValue currently caps at 3D,
// so all permutation vectors must have length 2 (matrix) or 3 (3D).
// Higher-dim is not representable and will throw.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <vector>

namespace numkit::m::builtin {

/// permute(A, perm) — perm is a 1-based permutation of [1..ndims(A)].
/// `B = permute(A, [2 1])` for a matrix is the transpose.
MValue permute(Allocator &alloc, const MValue &x, const std::vector<int> &perm);

/// ipermute — inverse of permute.
/// `ipermute(permute(A, p), p) == A` for any valid p.
MValue ipermute(Allocator &alloc, const MValue &x, const std::vector<int> &perm);

/// squeeze — drop singleton dimensions. Vectors and 2D matrices are
/// returned unchanged (MATLAB doesn't squeeze 2D below 2D); 3D arrays
/// with at least one singleton dim collapse down to 2D.
MValue squeeze(Allocator &alloc, const MValue &x);

/// cat(dim, A, B, ...) — concatenate along dim 1, 2, or 3.
/// dim=1 == vertcat, dim=2 == horzcat, dim=3 stacks 2D pages into 3D
/// (or extends an existing 3D's page count). All inputs must agree on
/// the non-`dim` dimensions.
MValue cat(Allocator &alloc, int dim, const MValue *values, size_t count);

/// blkdiag(A, B, C, ...) — block-diagonal matrix with the inputs on
/// the diagonal and zeros elsewhere. 2D inputs only.
MValue blkdiag(Allocator &alloc, const MValue *values, size_t count);

} // namespace numkit::m::builtin
