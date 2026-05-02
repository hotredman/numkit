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
Value repmat(std::pmr::memory_resource *mr, const Value &x,
              size_t m, size_t n, size_t p = 1);

/// ND repmat — tile vector of arbitrary length. Output rank =
/// max(input ndim, ntiles). Both input dims and tile vector are
/// padded to that rank with trailing 1s. DOUBLE inputs only for now.
Value repmatND(std::pmr::memory_resource *mr, const Value &x,
                const size_t *tiles, int ntiles);

/// Flip along columns (left-right). Each row is reversed.
Value fliplr(std::pmr::memory_resource *mr, const Value &x);

/// Flip along rows (up-down). Each column is reversed.
Value flipud(std::pmr::memory_resource *mr, const Value &x);

/// rot90(A) / rot90(A, k) — counter-clockwise 90° rotations of a 2D matrix.
/// k can be negative (clockwise). Modulo 4 cycles. ND inputs are
/// rotated per (R×C) slice (axes 2..N-1 are outer pages).
Value rot90(std::pmr::memory_resource *mr, const Value &x, int k = 1);

/// circshift(A, k) — circular shift along first non-singleton dim by k
/// (positive = down/right). circshift(A, [r c]) for 2D.
Value circshift(std::pmr::memory_resource *mr, const Value &x, int64_t k);
Value circshift(std::pmr::memory_resource *mr, const Value &x,
                 int64_t kRow, int64_t kCol);

/// ND circshift — shift vector of arbitrary length. shifts[i] applies to
/// axis i; entries past input rank are no-ops. Negative shifts wrap.
/// DOUBLE inputs only.
Value circshiftND(std::pmr::memory_resource *mr, const Value &x,
                   const int64_t *shifts, int nshifts);

/// Lower / upper triangular extraction. k is the diagonal offset
/// (0 = main, +1 = above main, -1 = below main). 2D only.
Value tril(std::pmr::memory_resource *mr, const Value &x, int k = 0);
Value triu(std::pmr::memory_resource *mr, const Value &x, int k = 0);

/// flip(A)         — flip along the first non-singleton dim.
/// flip(A, dim)    — flip along the given 1-based dim.
/// Type-preserving (DOUBLE / SINGLE / int / logical / cell / struct via
/// byte-copy on the elemental cell).
Value flip(std::pmr::memory_resource *mr, const Value &x, int dim1Based = 0);

/// repelem(v, n)        — vector v, scalar n: each element of v repeated
///                        n times consecutively.
/// repelem(A, m, n)     — matrix A, scalar counts: every entry expands
///                        to an m × n block of copies.
Value repelem(std::pmr::memory_resource *mr, const Value &x, size_t n);
Value repelem(std::pmr::memory_resource *mr, const Value &x, size_t m, size_t n);

} // namespace numkit::builtin
