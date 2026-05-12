// libs/builtin/include/numkit/builtin/language/arrays/manip.hpp
//
// Phase-5 array manipulation: repmat, fliplr, flipud, rot90,
// circshift, tril, triu. All work on 2D and 3D inputs.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <cstdint>

namespace numkit::builtin {

/// repmat(A, n)            — tile A into n×n grid (matrix), preserves 3D pages.
/// repmat(A, m, n)         — tile m × n.
/// repmat(A, m, n, p)      — tile m × n × p (3D).
Value repmat(const Value &x, size_t m, size_t n, size_t p = 1, std::pmr::memory_resource *mr = nullptr);

/// ND repmat — tile vector of arbitrary length. Output rank =
/// max(input ndim, ntiles). Both input dims and tile vector are
/// padded to that rank with trailing 1s. DOUBLE inputs only for now.
Value repmatND(const Value &x, const size_t *tiles, int ntiles, std::pmr::memory_resource *mr = nullptr);

/// Flip along columns (left-right). Each row is reversed.
Value fliplr(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Flip along rows (up-down). Each column is reversed.
Value flipud(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// rot90(A) / rot90(A, k) — counter-clockwise 90° rotations of a 2D matrix.
/// k can be negative (clockwise). Modulo 4 cycles. ND inputs are
/// rotated per (R×C) slice (axes 2..N-1 are outer pages).
Value rot90(const Value &x, int k = 1, std::pmr::memory_resource *mr = nullptr);

/// circshift(A, k) — circular shift along first non-singleton dim by k
/// (positive = down/right). circshift(A, [r c]) for 2D.
Value circshift(const Value &x, int64_t k, std::pmr::memory_resource *mr = nullptr);
Value circshift(const Value &x, int64_t kRow, int64_t kCol, std::pmr::memory_resource *mr = nullptr);

/// ND circshift — shift vector of arbitrary length. shifts[i] applies to
/// axis i; entries past input rank are no-ops. Negative shifts wrap.
/// DOUBLE inputs only.
Value circshiftND(const Value &x, const int64_t *shifts, int nshifts, std::pmr::memory_resource *mr = nullptr);

/// Lower / upper triangular extraction. k is the diagonal offset
/// (0 = main, +1 = above main, -1 = below main). 2D only.
Value tril(const Value &x, int k = 0, std::pmr::memory_resource *mr = nullptr);
Value triu(const Value &x, int k = 0, std::pmr::memory_resource *mr = nullptr);

/// flip(A)         — flip along the first non-singleton dim.
/// flip(A, dim)    — flip along the given 1-based dim.
/// Type-preserving (DOUBLE / SINGLE / int / logical / cell / struct via
/// byte-copy on the elemental cell).
Value flip(const Value &x, int dim1Based = 0, std::pmr::memory_resource *mr = nullptr);

/// repelem(v, n)        — vector v, scalar n: each element of v repeated
///                        n times consecutively.
/// repelem(A, m, n)     — matrix A, scalar counts: every entry expands
///                        to an m × n block of copies.
Value repelem(const Value &x, size_t n, std::pmr::memory_resource *mr = nullptr);
Value repelem(const Value &x, size_t m, size_t n, std::pmr::memory_resource *mr = nullptr);

// ── Pack 32: array shape pads (vectors only) ─────────────────────────
/// paddata(v, n) — pad v with zeros (trailing) until length == n.
/// If numel(v) ≥ n, returns v unchanged.
Value paddata(const Value &v, size_t n, std::pmr::memory_resource *mr = nullptr);
/// trimdata(v, n) — truncate v from the trailing end to length n.
Value trimdata(const Value &v, size_t n, std::pmr::memory_resource *mr = nullptr);
/// resize(v, n) — pad-or-trim to exactly length n.
Value resize(const Value &v, size_t n, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
