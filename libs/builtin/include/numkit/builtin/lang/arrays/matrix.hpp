// libs/builtin/include/numkit/builtin/lang/arrays/matrix.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>
#include <vector>

namespace numkit::builtin {

// ── Constructors ──────────────────────────────────────────────────────
/// All-zero matrix. pages == 0 → 2D matrix.
Value zeros(std::pmr::memory_resource *mr, size_t rows, size_t cols = 1, size_t pages = 0);
Value ones(std::pmr::memory_resource *mr, size_t rows, size_t cols = 1, size_t pages = 0);

/// Identity matrix. rows, cols may differ — produces rectangular "identity".
Value eye(std::pmr::memory_resource *mr, size_t rows, size_t cols);

// ── Shape queries ────────────────────────────────────────────────────
/// size(x) returns a row vector of dimensions.
/// @param asVector  when true, returns [rows, cols] or [rows, cols, pages].
///                  For nargout > 1 form, call sizePair or sizeTriple below.
Value size(std::pmr::memory_resource *mr, const Value &x);

/// size(x, dim) — scalar = dim'th dimension (1-based).
Value size(std::pmr::memory_resource *mr, const Value &x, int dim);

/// size(x) into separate rows/cols pair (MATLAB [r, c] = size(x)).
std::tuple<Value, Value> sizePair(std::pmr::memory_resource *mr, const Value &x);

/// length(x) = max of all dimensions; 0 if empty.
Value length(std::pmr::memory_resource *mr, const Value &x);

/// numel(x) = total element count.
Value numel(std::pmr::memory_resource *mr, const Value &x);

/// ndims(x) = number of dimensions (2 for matrix, 3 for 3D array).
Value ndims(std::pmr::memory_resource *mr, const Value &x);

// ── Shape transformations ────────────────────────────────────────────
/// Reshape preserving column-major element order. totalNumel must match:
/// numel(x) == rows * cols * (pages == 0 ? 1 : pages). pages == 0 means 2D output.
/// For dimension inference (MATLAB's [] placeholder), resolve in caller
/// before invoking — this function requires concrete dims.
Value reshape(std::pmr::memory_resource *mr, const Value &x, size_t rows, size_t cols, size_t pages = 0);

/// ND reshape — accepts a flat dim list of arbitrary rank (≥ 1). Same
/// elem-count check as the 2D/3D form. CELL/STRING reshape past 3D is
/// not yet supported (throws m:reshape:cellND). Pointer + size so the
/// same overload composes with std::vector / std::pmr::vector / arrays.
Value reshapeND(std::pmr::memory_resource *mr, const Value &x,
                const size_t *dims, std::size_t nDims);

/// 2D matrix transpose (no complex conjugation). Throws Error on 3D input.
Value transpose(std::pmr::memory_resource *mr, const Value &x);

/// Page-wise matrix multiply: treats axes 1-2 as M×K / K×N matrices,
/// axes ≥3 as batch dims. Output shape is [M, N, ...broadcast(batchX, batchY)].
/// DOUBLE only. Inner dim mismatch throws.
///
/// Transpose flags map MATLAB strings: "none" = no op, "transpose" =
/// per-page transpose, "ctranspose" = per-page conjugate-transpose
/// (identical to transpose for real input; complex input not yet
/// supported).
enum class TranspOp { None, Transpose, CTranspose };
Value pagemtimes(std::pmr::memory_resource *mr, const Value &x, const Value &y);
Value pagemtimes(std::pmr::memory_resource *mr,
                  const Value &x, TranspOp tx,
                  const Value &y, TranspOp ty);

/// Main diagonal of a matrix as a column vector, or vector → diagonal matrix.
Value diag(std::pmr::memory_resource *mr, const Value &x);

// ── Sort / find ──────────────────────────────────────────────────────
/// Sort along first non-singleton dimension; returns (sorted, indices).
/// Indices are 1-based permutation. For 3D input, operates per-slice.
std::tuple<Value, Value> sort(std::pmr::memory_resource *mr, const Value &x);

/// sortrows(M) — lex-sort the rows of a 2D matrix in ascending order
/// across all columns (column 1 most significant). Stable sort.
/// Returns (sorted, idx) where idx is the 1-based original row order.
/// `cols` form: each entry is a 1-based column index; negative entries
/// flip direction for that key (descending). Empty `cols` means "all
/// columns ascending" (same as the 1-arg form).
/// Promotes integer/logical input to DOUBLE.
std::tuple<Value, Value> sortrows(std::pmr::memory_resource *mr, const Value &x);
std::tuple<Value, Value> sortrows(std::pmr::memory_resource *mr, const Value &x,
                                    const int *cols, std::size_t nCols);

/// Linear indices of non-zero (or true) entries. Result is a row vector
/// when x is a row, column vector otherwise.
Value find(std::pmr::memory_resource *mr, const Value &x);

/// nnz(x) — number of non-zero elements. NaN counts as non-zero
/// (NaN != 0). For COMPLEX, an element is non-zero iff real or imag
/// part is non-zero. Returns DOUBLE scalar.
Value nnz(std::pmr::memory_resource *mr, const Value &x);

/// nonzeros(x) — column vector of non-zero elements in column-major
/// order. Output type matches input type (DOUBLE/SINGLE/COMPLEX/INT*/
/// LOGICAL preserved).
Value nonzeros(std::pmr::memory_resource *mr, const Value &x);

// ── Concatenation ────────────────────────────────────────────────────
/// Horizontal concatenation (along columns).
Value horzcat(std::pmr::memory_resource *mr, const Value *values, size_t count);

/// Vertical concatenation (along rows).
Value vertcat(std::pmr::memory_resource *mr, const Value *values, size_t count);

// ── Grids ────────────────────────────────────────────────────────────
/// meshgrid(x, y) returns (X, Y) matrices of size [ny, nx].
std::tuple<Value, Value> meshgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y);

/// ndgrid(x, y) — N-D companion to meshgrid. Each output has shape
/// [numel(x), numel(y), ...] (first-arg axes-major) — the opposite
/// of meshgrid's MATLAB convention. Output type DOUBLE.
std::tuple<Value, Value>
ndgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y);

/// 3-input ndgrid(x, y, z) — outputs have shape [numel(x), numel(y), numel(z)].
std::tuple<Value, Value, Value>
ndgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y, const Value &z);

/// kron(A, B) — Kronecker product. Output is (rA*rB) × (cA*cB);
/// the (i, j)-th block (rB × cB) equals A[i, j] · B. Vector inputs
/// are treated as matrices of their natural orientation. DOUBLE only
/// for now (integer/logical/single promoted; complex throws).
Value kron(std::pmr::memory_resource *mr, const Value &a, const Value &b);

// ── Reductions and products ──────────────────────────────────────────
//
// Cumulative ops keep the input shape; sum/prod/max/min along the
// chosen dim. Two-arg form auto-detects the first non-singleton dim;
// three-arg form takes an explicit 1-based dim (0 = auto).
Value cumsum (std::pmr::memory_resource *mr, const Value &x);
Value cumsum (std::pmr::memory_resource *mr, const Value &x, int dim);
Value cumprod(std::pmr::memory_resource *mr, const Value &x, int dim = 0);
Value cummax (std::pmr::memory_resource *mr, const Value &x, int dim = 0);
Value cummin (std::pmr::memory_resource *mr, const Value &x, int dim = 0);

// diff(x[, n[, dim]]) — n-th order discrete difference along dim.
// out[i] = x[i+1] - x[i]. Output shape: input with dim[d-1] decremented
// by n (clamped to 0). n=0 returns a copy. Default dim = first non-
// singleton. Scalar input returns 1×0 empty (MATLAB convention).
Value diff(std::pmr::memory_resource *mr, const Value &x, int n = 1, int dim = 0);

// Logical reductions: collapse the chosen dim to a single 0/1 value.
// Empty slices: any → false, all → true (matches MATLAB).
// Output type is LOGICAL.
Value anyOf(std::pmr::memory_resource *mr, const Value &x, int dim = 0);
Value allOf(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

// Elementwise xor — both inputs treated as boolean (non-zero = true).
// Output type is LOGICAL. Standard broadcasting rules apply.
Value xorOf(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// Cross product of 3-element vectors. Row vector output.
Value cross(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// Dot product of two vectors of equal length.
Value dot(std::pmr::memory_resource *mr, const Value &a, const Value &b);

} // namespace numkit::builtin
