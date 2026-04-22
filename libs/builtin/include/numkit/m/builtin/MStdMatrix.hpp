// libs/builtin/include/numkit/m/builtin/MStdMatrix.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::builtin {

// ── Constructors ──────────────────────────────────────────────────────
/// All-zero matrix. pages == 0 → 2D matrix.
MValue zeros(Allocator &alloc, size_t rows, size_t cols = 1, size_t pages = 0);
MValue ones(Allocator &alloc, size_t rows, size_t cols = 1, size_t pages = 0);

/// Identity matrix. rows, cols may differ — produces rectangular "identity".
MValue eye(Allocator &alloc, size_t rows, size_t cols);

// ── Shape queries ────────────────────────────────────────────────────
/// size(x) returns a row vector of dimensions.
/// @param asVector  when true, returns [rows, cols] or [rows, cols, pages].
///                  For nargout > 1 form, call sizePair or sizeTriple below.
MValue size(Allocator &alloc, const MValue &x);

/// size(x, dim) — scalar = dim'th dimension (1-based).
MValue size(Allocator &alloc, const MValue &x, int dim);

/// size(x) into separate rows/cols pair (MATLAB [r, c] = size(x)).
std::tuple<MValue, MValue> sizePair(Allocator &alloc, const MValue &x);

/// length(x) = max of all dimensions; 0 if empty.
MValue length(Allocator &alloc, const MValue &x);

/// numel(x) = total element count.
MValue numel(Allocator &alloc, const MValue &x);

/// ndims(x) = number of dimensions (2 for matrix, 3 for 3D array).
MValue ndims(Allocator &alloc, const MValue &x);

// ── Shape transformations ────────────────────────────────────────────
/// Reshape preserving column-major element order. totalNumel must match:
/// numel(x) == rows * cols * (pages == 0 ? 1 : pages). pages == 0 means 2D output.
/// For dimension inference (MATLAB's [] placeholder), resolve in caller
/// before invoking — this function requires concrete dims.
MValue reshape(Allocator &alloc, const MValue &x, size_t rows, size_t cols, size_t pages = 0);

/// 2D matrix transpose (no complex conjugation). Throws MError on 3D input.
MValue transpose(Allocator &alloc, const MValue &x);

/// Main diagonal of a matrix as a column vector, or vector → diagonal matrix.
MValue diag(Allocator &alloc, const MValue &x);

// ── Sort / find ──────────────────────────────────────────────────────
/// Sort along first non-singleton dimension; returns (sorted, indices).
/// Indices are 1-based permutation. For 3D input, operates per-slice.
std::tuple<MValue, MValue> sort(Allocator &alloc, const MValue &x);

/// Linear indices of non-zero (or true) entries. Result is a row vector
/// when x is a row, column vector otherwise.
MValue find(Allocator &alloc, const MValue &x);

// ── Concatenation ────────────────────────────────────────────────────
/// Horizontal concatenation (along columns).
MValue horzcat(Allocator &alloc, const MValue *values, size_t count);

/// Vertical concatenation (along rows).
MValue vertcat(Allocator &alloc, const MValue *values, size_t count);

// ── Grids ────────────────────────────────────────────────────────────
/// meshgrid(x, y) returns (X, Y) matrices of size [ny, nx].
std::tuple<MValue, MValue> meshgrid(Allocator &alloc, const MValue &x, const MValue &y);

// ── Reductions and products ──────────────────────────────────────────
/// Cumulative sum along first non-singleton dimension.
MValue cumsum(Allocator &alloc, const MValue &x);

/// Cumulative sum along an explicit 1-based dim. dim==0 → auto-detect.
MValue cumsum(Allocator &alloc, const MValue &x, int dim);

/// Cross product of 3-element vectors. Row vector output.
MValue cross(Allocator &alloc, const MValue &a, const MValue &b);

/// Dot product of two vectors of equal length.
MValue dot(Allocator &alloc, const MValue &a, const MValue &b);

} // namespace numkit::m::builtin
