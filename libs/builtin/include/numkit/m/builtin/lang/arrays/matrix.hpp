// libs/builtin/include/numkit/m/builtin/lang/arrays/matrix.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>
#include <vector>

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

/// ND reshape — accepts a dim vector of arbitrary rank (≥ 1). Same
/// elem-count check as the 2D/3D form. CELL/STRING reshape past 3D is
/// not yet supported (throws m:reshape:cellND).
MValue reshapeND(Allocator &alloc, const MValue &x, const std::vector<size_t> &dims);

/// 2D matrix transpose (no complex conjugation). Throws MError on 3D input.
MValue transpose(Allocator &alloc, const MValue &x);

/// Page-wise matrix multiply: treats axes 1-2 as M×K / K×N matrices,
/// axes ≥3 as batch dims. Output shape is [M, N, ...broadcast(batchX, batchY)].
/// DOUBLE only. Inner dim mismatch throws.
///
/// Transpose flags map MATLAB strings: "none" = no op, "transpose" =
/// per-page transpose, "ctranspose" = per-page conjugate-transpose
/// (identical to transpose for real input; complex input not yet
/// supported).
enum class TranspOp { None, Transpose, CTranspose };
MValue pagemtimes(Allocator &alloc, const MValue &x, const MValue &y);
MValue pagemtimes(Allocator &alloc,
                  const MValue &x, TranspOp tx,
                  const MValue &y, TranspOp ty);

/// Main diagonal of a matrix as a column vector, or vector → diagonal matrix.
MValue diag(Allocator &alloc, const MValue &x);

// ── Sort / find ──────────────────────────────────────────────────────
/// Sort along first non-singleton dimension; returns (sorted, indices).
/// Indices are 1-based permutation. For 3D input, operates per-slice.
std::tuple<MValue, MValue> sort(Allocator &alloc, const MValue &x);

/// sortrows(M) — lex-sort the rows of a 2D matrix in ascending order
/// across all columns (column 1 most significant). Stable sort.
/// Returns (sorted, idx) where idx is the 1-based original row order.
/// `cols` form: each entry is a 1-based column index; negative entries
/// flip direction for that key (descending). Empty `cols` means "all
/// columns ascending" (same as the 1-arg form).
/// Promotes integer/logical input to DOUBLE.
std::tuple<MValue, MValue> sortrows(Allocator &alloc, const MValue &x);
std::tuple<MValue, MValue> sortrows(Allocator &alloc, const MValue &x,
                                    const std::vector<int> &cols);

/// Linear indices of non-zero (or true) entries. Result is a row vector
/// when x is a row, column vector otherwise.
MValue find(Allocator &alloc, const MValue &x);

/// nnz(x) — number of non-zero elements. NaN counts as non-zero
/// (NaN != 0). For COMPLEX, an element is non-zero iff real or imag
/// part is non-zero. Returns DOUBLE scalar.
MValue nnz(Allocator &alloc, const MValue &x);

/// nonzeros(x) — column vector of non-zero elements in column-major
/// order. Output type matches input type (DOUBLE/SINGLE/COMPLEX/INT*/
/// LOGICAL preserved).
MValue nonzeros(Allocator &alloc, const MValue &x);

// ── Concatenation ────────────────────────────────────────────────────
/// Horizontal concatenation (along columns).
MValue horzcat(Allocator &alloc, const MValue *values, size_t count);

/// Vertical concatenation (along rows).
MValue vertcat(Allocator &alloc, const MValue *values, size_t count);

// ── Grids ────────────────────────────────────────────────────────────
/// meshgrid(x, y) returns (X, Y) matrices of size [ny, nx].
std::tuple<MValue, MValue> meshgrid(Allocator &alloc, const MValue &x, const MValue &y);

/// ndgrid(x, y) — N-D companion to meshgrid. Each output has shape
/// [numel(x), numel(y), ...] (first-arg axes-major) — the opposite
/// of meshgrid's MATLAB convention. Output type DOUBLE.
std::tuple<MValue, MValue>
ndgrid(Allocator &alloc, const MValue &x, const MValue &y);

/// 3-input ndgrid(x, y, z) — outputs have shape [numel(x), numel(y), numel(z)].
std::tuple<MValue, MValue, MValue>
ndgrid(Allocator &alloc, const MValue &x, const MValue &y, const MValue &z);

/// kron(A, B) — Kronecker product. Output is (rA*rB) × (cA*cB);
/// the (i, j)-th block (rB × cB) equals A[i, j] · B. Vector inputs
/// are treated as matrices of their natural orientation. DOUBLE only
/// for now (integer/logical/single promoted; complex throws).
MValue kron(Allocator &alloc, const MValue &a, const MValue &b);

// ── Reductions and products ──────────────────────────────────────────
//
// Cumulative ops keep the input shape; sum/prod/max/min along the
// chosen dim. Two-arg form auto-detects the first non-singleton dim;
// three-arg form takes an explicit 1-based dim (0 = auto).
MValue cumsum (Allocator &alloc, const MValue &x);
MValue cumsum (Allocator &alloc, const MValue &x, int dim);
MValue cumprod(Allocator &alloc, const MValue &x, int dim = 0);
MValue cummax (Allocator &alloc, const MValue &x, int dim = 0);
MValue cummin (Allocator &alloc, const MValue &x, int dim = 0);

// diff(x[, n[, dim]]) — n-th order discrete difference along dim.
// out[i] = x[i+1] - x[i]. Output shape: input with dim[d-1] decremented
// by n (clamped to 0). n=0 returns a copy. Default dim = first non-
// singleton. Scalar input returns 1×0 empty (MATLAB convention).
MValue diff(Allocator &alloc, const MValue &x, int n = 1, int dim = 0);

// Logical reductions: collapse the chosen dim to a single 0/1 value.
// Empty slices: any → false, all → true (matches MATLAB).
// Output type is LOGICAL.
MValue anyOf(Allocator &alloc, const MValue &x, int dim = 0);
MValue allOf(Allocator &alloc, const MValue &x, int dim = 0);

// Elementwise xor — both inputs treated as boolean (non-zero = true).
// Output type is LOGICAL. Standard broadcasting rules apply.
MValue xorOf(Allocator &alloc, const MValue &a, const MValue &b);

/// Cross product of 3-element vectors. Row vector output.
MValue cross(Allocator &alloc, const MValue &a, const MValue &b);

/// Dot product of two vectors of equal length.
MValue dot(Allocator &alloc, const MValue &a, const MValue &b);

} // namespace numkit::m::builtin
