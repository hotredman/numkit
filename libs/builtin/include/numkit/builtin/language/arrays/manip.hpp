// libs/builtin/include/numkit/builtin/language/arrays/manip.hpp
//
// Phase-5 array manipulation: repmat, fliplr, flipud, rot90,
// circshift, tril, triu, flip, repelem, paddata, trimdata, resize.

#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <cstdint>
#include <vector>

namespace numkit::builtin {

/// @brief Tile an array (`B = repmat(A, m, n, p)`).
///
/// Tiles `A` into an `m × n × p` arrangement.
/// - `repmat(A, n)` is `m = n = p = 1` (square form).
/// - `repmat(A, m, n)` keeps 2-D.
/// - `repmat(A, m, n, p)` produces 3-D when `p > 1`.
///
/// @param x   Input array.
/// @param m   Row tile count.
/// @param n   Column tile count.
/// @param p   Page tile count (default 1 → 2-D output).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tiled array.
/// @see repmatND, repelem
Value repmat(const Value &x, size_t m, size_t n, size_t p = 1,
             std::pmr::memory_resource *mr = nullptr);

/// @brief ND tile (`B = repmatND(A, tiles)`).
///
/// Tiles `A` by the per-axis vector `tiles`. Output rank =
/// `max(input ndim, ntiles)`. Both input dims and tile vector are
/// padded with trailing 1s. DOUBLE inputs only currently.
///
/// @param x      Input array.
/// @param tiles  Per-axis tile counts.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Tiled array.
/// @see repmat
Value repmatND(const Value &x, Span<const size_t> tiles,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Flip along columns (`B = fliplr(A)`).
///
/// Reverses each row.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column-flipped array.
/// @see flipud, flip, rot90
Value fliplr(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Flip along rows (`B = flipud(A)`).
///
/// Reverses each column.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row-flipped array.
/// @see fliplr, flip
Value flipud(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief 90° rotation (`B = rot90(A, k)`).
///
/// Counter-clockwise rotation by `k · 90°`. Negative `k` rotates
/// clockwise. `k` is taken mod 4. ND inputs are rotated per `R × C`
/// slice (axes `2..N-1` are outer pages).
///
/// @param x   Input array (≥ 2-D).
/// @param k   Number of 90° steps (default 1).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Rotated array.
/// @see fliplr, flipud, flip
Value rot90(const Value &x, int k = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Circular shift along the first non-singleton dim
/// (`B = circshift(A, k)`).
///
/// Positive `k` shifts down / right; negative shifts wrap to the
/// opposite end.
///
/// @param x   Input array.
/// @param k   Shift amount.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Shifted array.
/// @see circshift(x, kRow, kCol, mr), circshiftND
Value circshift(const Value &x, int64_t k, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D circular shift (`B = circshift(A, [r c])`).
///
/// @param x     Input matrix.
/// @param kRow  Row shift (positive = down).
/// @param kCol  Column shift (positive = right).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Shifted matrix.
/// @see circshift(x, k, mr)
Value circshift(const Value &x, int64_t kRow, int64_t kCol,
                std::pmr::memory_resource *mr = nullptr);

/// @brief ND circular shift (`B = circshiftND(A, shifts)`).
///
/// `shifts[i]` applies to axis `i`. Entries past input rank are no-ops.
/// Negative shifts wrap. DOUBLE inputs only.
///
/// @param x       Input array.
/// @param shifts  Per-axis shift amounts.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Shifted array.
/// @see circshift
Value circshiftND(const Value &x, Span<const int64_t> shifts,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Lower-triangular part (`B = tril(A, k)`).
///
/// `k = 0` keeps the main diagonal and below; `k > 0` keeps additional
/// super-diagonals; `k < 0` drops sub-diagonals. 2-D only.
///
/// @param x   Input matrix.
/// @param k   Diagonal offset.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Lower-triangular extract.
/// @see triu
Value tril(const Value &x, int k = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Upper-triangular part (`B = triu(A, k)`).
///
/// `k = 0` keeps the main diagonal and above; symmetric to @ref tril.
///
/// @param x   Input matrix.
/// @param k   Diagonal offset.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Upper-triangular extract.
/// @see tril
Value triu(const Value &x, int k = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Flip along the chosen dimension
/// (`B = flip(A, dim1Based)`).
///
/// `dim1Based == 0` → flip along the first non-singleton dim.
/// Type-preserving: DOUBLE / SINGLE / int / LOGICAL / CELL / STRUCT
/// (via byte-copy on the elemental cell).
///
/// @param x          Input array.
/// @param dim1Based  1-based dimension (0 → auto).
/// @param mr         Memory resource (nullptr → process default).
/// @return           Flipped array.
/// @see fliplr, flipud
Value flip(const Value &x, int dim1Based = 0,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Per-element replication for a vector (`y = repelem(v, n)`).
///
/// Each element of vector `v` repeated `n` times consecutively.
///
/// @param x   Input vector.
/// @param n   Repeat count per element.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Expanded vector of length `numel(v) · n`.
/// @see repelem(x, m, n, mr), repmat
Value repelem(const Value &x, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Per-element block replication for a matrix
/// (`Y = repelem(A, m, n)`).
///
/// Every entry of `A` expands to an `m × n` block of copies in `Y`.
///
/// @param x   Input matrix.
/// @param m   Per-element row count.
/// @param n   Per-element column count.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Expanded matrix.
/// @see repmat
Value repelem(const Value &x, size_t m, size_t n,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Per-element replication with per-element counts
/// (`y = repelem(v, counts)`).
///
/// `counts` is a scalar (every element repeated that many times — same as
/// `repelem(v, n)`) or a DOUBLE vector the same length as `v`, giving the
/// repeat count for each element. A zero count drops that element. Result
/// orientation matches `v` (scalar → row).
///
/// @param x       Input vector (or scalar).
/// @param counts  Scalar or per-element count vector.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Expanded vector of length `sum(counts)`.
Value repelem(const Value &x, const Value &counts,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Per-element block replication with per-row/column counts
/// (`Y = repelem(A, r, c)`).
///
/// `r` and `c` are each a scalar or a DOUBLE vector (length `size(A,1)` /
/// `size(A,2)`) giving the per-row / per-column replication count.
///
/// @param x        Input matrix.
/// @param rCounts  Scalar or per-row count vector.
/// @param cCounts  Scalar or per-column count vector.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Expanded matrix.
Value repelem(const Value &x, const Value &rCounts, const Value &cCounts,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Pad a vector with zeros to length `n` (`v = paddata(v, n)`).
///
/// Trailing zero-pad. If `numel(v) >= n`, returns `v` unchanged.
///
/// @param v   Input vector.
/// @param n   Target length.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Padded vector.
/// @see trimdata, resize
Value paddata(const Value &v, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Truncate a vector to length `n` (`v = trimdata(v, n)`).
///
/// Drops trailing elements. If `numel(v) <= n`, returns `v` unchanged.
///
/// @param v   Input vector.
/// @param n   Target length.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Trimmed vector.
/// @see paddata, resize
Value trimdata(const Value &v, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Pad-or-trim to exact length `n` (`v = resize(v, n)`).
///
/// Combines @ref paddata and @ref trimdata.
///
/// @param v   Input vector.
/// @param n   Target length.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Vector of length exactly `n`.
Value resize(const Value &v, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Subscripts → linear index (`ind = sub2ind(siz, sub1, sub2, …)`).
///
/// Converts the (column-major) subscript arrays `subs` into linear indices
/// for an array of size `siz`. The subscript arrays must all have the same
/// shape (or be scalars); the result inherits that shape. Fewer subscripts
/// than `numel(siz)` dimensions is allowed (missing higher dims default to
/// subscript 1).
///
/// @param siz   Size vector (per-dimension extents).
/// @param subs  One subscript array per dimension (variadic; same shape).
/// @param mr    Memory resource (nullptr → process default).
/// @return      1-based linear indices, shaped like the subscript arrays.
/// @throws Error if `siz` is empty, `subs` is empty, there are more
///         subscript arrays than dimensions, or the subscript shapes differ.
/// @see ind2sub
Value sub2ind(const Value &siz, Span<const Value> subs,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Linear index → subscripts (`[s1, s2, …] = ind2sub(siz, ind)`).
///
/// Inverse of @ref sub2ind: decomposes the 1-based linear indices `ind`
/// into `nout` subscript arrays for an array of size `siz`. When `nout` is
/// fewer than `numel(siz)`, the last output absorbs the remaining
/// dimensions (column-major). `nout == 0` (default) uses `numel(siz)`.
///
/// @param siz   Size vector (per-dimension extents).
/// @param ind   1-based linear indices.
/// @param nout  Number of subscript outputs (`0` → `numel(siz)`).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `nout` subscript arrays, each shaped like `ind`.
/// @throws Error if `siz` is empty.
/// @see sub2ind
std::vector<Value> ind2sub(const Value &siz, const Value &ind, size_t nout = 0,
                           std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
