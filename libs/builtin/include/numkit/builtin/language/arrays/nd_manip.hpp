// libs/builtin/include/numkit/builtin/language/arrays/nd_manip.hpp
//
// Phase-6 N-D array manipulation: permute / ipermute / squeeze /
// cat(dim, ...) / blkdiag. numkit-m's Value currently caps at 3D,
// so all permutation vectors must have length 2 (matrix) or 3 (3D).
// Higher-dim is not representable and will throw.

#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <cstddef>

namespace numkit::builtin {

/// permute(A, perm) — perm is a 1-based permutation of [1..ndims(A)].
/// `B = permute(A, [2 1])` for a matrix is the transpose. Pointer + size
/// so the same overload composes with std::vector / pmr::vector / arrays.
Value permute(const Value &x, const int *perm, std::size_t n, std::pmr::memory_resource *mr = nullptr);

/// ipermute — inverse of permute.
/// `ipermute(permute(A, p), p) == A` for any valid p.
Value ipermute(const Value &x, const int *perm, std::size_t n, std::pmr::memory_resource *mr = nullptr);

/// squeeze — drop singleton dimensions. Vectors and 2D matrices are
/// returned unchanged (MATLAB doesn't squeeze 2D below 2D); 3D arrays
/// with at least one singleton dim collapse down to 2D.
Value squeeze(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// cat(dim, A, B, ...) — concatenate along dim 1, 2, or 3.
/// dim=1 == vertcat, dim=2 == horzcat, dim=3 stacks 2D pages into 3D
/// (or extends an existing 3D's page count). All inputs must agree on
/// the non-`dim` dimensions.
Value cat(int dim, Span<const Value> values, std::pmr::memory_resource *mr = nullptr);

/// blkdiag(A, B, C, ...) — block-diagonal matrix with the inputs on
/// the diagonal and zeros elsewhere. 2D inputs only.
Value blkdiag(Span<const Value> values, std::pmr::memory_resource *mr = nullptr);

/// shiftdim(A, n) — cyclic-left shift of dim ordering by n. For n > 0:
/// equivalent to permute with `[n+1, n+2, ..., N, 1, 2, ..., n]`. For
/// n < 0: prepend |n| singleton dimensions. n is reduced mod N when
/// it equals or exceeds N.
Value shiftdim(const Value &x, int n, std::pmr::memory_resource *mr = nullptr);

/// Auto form `[B, k] = shiftdim(A)` — drop leading singleton dims and
/// report how many were dropped. If no leading singletons, returns
/// {A, 0}.
struct ShiftDimAuto { Value v; int dropped; };
ShiftDimAuto shiftdimAuto(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
