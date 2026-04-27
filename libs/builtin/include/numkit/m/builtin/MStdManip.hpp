// libs/builtin/include/numkit/m/builtin/MStdManip.hpp
//
// Phase-5 array manipulation: repmat, fliplr, flipud, rot90,
// circshift, tril, triu. All work on 2D and 3D inputs.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <cstdint>

namespace numkit::m::builtin {

/// repmat(A, n)            — tile A into n×n grid (matrix), preserves 3D pages.
/// repmat(A, m, n)         — tile m × n.
/// repmat(A, m, n, p)      — tile m × n × p (3D).
MValue repmat(Allocator &alloc, const MValue &x,
              size_t m, size_t n, size_t p = 1);

/// ND repmat — tile vector of arbitrary length. Output rank =
/// max(input ndim, ntiles). Both input dims and tile vector are
/// padded to that rank with trailing 1s. DOUBLE inputs only for now.
MValue repmatND(Allocator &alloc, const MValue &x,
                const size_t *tiles, int ntiles);

/// Flip along columns (left-right). Each row is reversed.
MValue fliplr(Allocator &alloc, const MValue &x);

/// Flip along rows (up-down). Each column is reversed.
MValue flipud(Allocator &alloc, const MValue &x);

/// rot90(A) / rot90(A, k) — counter-clockwise 90° rotations of a 2D matrix.
/// k can be negative (clockwise). Modulo 4 cycles. ND inputs are
/// rotated per (R×C) slice (axes 2..N-1 are outer pages).
MValue rot90(Allocator &alloc, const MValue &x, int k = 1);

/// circshift(A, k) — circular shift along first non-singleton dim by k
/// (positive = down/right). circshift(A, [r c]) for 2D.
MValue circshift(Allocator &alloc, const MValue &x, int64_t k);
MValue circshift(Allocator &alloc, const MValue &x,
                 int64_t kRow, int64_t kCol);

/// ND circshift — shift vector of arbitrary length. shifts[i] applies to
/// axis i; entries past input rank are no-ops. Negative shifts wrap.
/// DOUBLE inputs only.
MValue circshiftND(Allocator &alloc, const MValue &x,
                   const int64_t *shifts, int nshifts);

/// Lower / upper triangular extraction. k is the diagonal offset
/// (0 = main, +1 = above main, -1 = below main). 2D only.
MValue tril(Allocator &alloc, const MValue &x, int k = 0);
MValue triu(Allocator &alloc, const MValue &x, int k = 0);

} // namespace numkit::m::builtin
