// libs/signal/include/numkit/signal/convolution/extras.hpp
//
// Convolution / correlation extras (E1):
//   cconv(x, y[, n])           — circular convolution
//   convmtx(h, n)              — convolution matrix
//   xcorr2(A, B)               — 2-D cross-correlation
//   finddelay(x, y[, max])     — lag of max(xcorr(x, y))
//   alignsignals(x, y[, max])  — align two signals by zero-padding

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Circular (cyclic) convolution of two real vectors.
///
/// Computes \f$ out[k] = \sum_{m=0}^{N-1} x[m] \cdot y[(k - m) \bmod N] \f$
/// for k = 0..N-1. Inputs are zero-padded to length N before the cyclic
/// wrap. When `n == 0` (default), N = max(numel(x), numel(y)).
///
/// FFT-based fast path is used when n ≥ numel(x) + numel(y) - 1 (the
/// length at which circular conv collapses to linear conv); otherwise
/// the direct O(N²) loop is used.
///
/// @param x   First real vector (any length).
/// @param y   Second real vector (any length).
/// @param n   Output / wrap length. `0` ≡ `max(numel(x), numel(y))`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    1×n DOUBLE row vector.
///
/// @code  Value c = cconv({1, 2, 3, 4}, {1, 1, 1});  // length 4 cyclic conv  @endcode
Value cconv(const Value &                x,
            const Value &                y,
            size_t                       n  = 0,
            std::pmr::memory_resource *  mr = nullptr);

/// Convolution matrix for an impulse response.
///
/// `convmtx(h, n)` returns a matrix M such that `M * x == conv(h, x)`
/// for any column vector `x` of length n. The shape depends on the
/// orientation of `h`:
///   * `h` is a row    → returns n × (n + nh - 1); each row is `h` shifted right.
///   * `h` is a column → returns (n + nh - 1) × n; each column is `h` shifted down.
///
/// where `nh = numel(h)`.
///
/// @param h   Impulse response, real row or column vector. Must be non-empty.
/// @param n   Number of shifts (must be > 0).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Convolution matrix per the shape rules above.
/// @throws    numkit::Error  if `n == 0` or `h` is empty.
Value convmtx(const Value &                h,
              size_t                       n,
              std::pmr::memory_resource *  mr = nullptr);

/// 2-D cross-correlation of two real matrices.
///
/// Output size is `(rA + rB - 1) × (cA + cB - 1)`. Equivalent to
/// `conv2(A, fliplr(flipud(B)), 'full')`. Uses an FFT path for large
/// inputs (when `rA·cA·rB·cB > 8192` and output ≥ 8×8) and the direct
/// O(rA·cA·rB·cB) loop otherwise.
///
/// @param A   First 2-D matrix.
/// @param B   Second 2-D matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE matrix of size (rA+rB-1) × (cA+cB-1).
Value xcorr2(const Value &                A,
             const Value &                B,
             std::pmr::memory_resource *  mr = nullptr);

/// Estimate the integer delay between two signals.
///
/// Returns the lag `d` (in samples) such that `x(t) ≈ y(t - d)`. A
/// positive `d` means `y` lags `x`. Computed as the lag corresponding
/// to the maximum of the cross-correlation `xcorr(x, y)`.
///
/// @param x         First signal.
/// @param y         Second signal.
/// @param max_lag   Cap on the search range in samples. `0` (default) →
///                  full range, `±max(nx, ny) - 1`.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Integer lag in samples (signed).
long finddelay(const Value &                x,
               const Value &                y,
               long                         max_lag = 0,
               std::pmr::memory_resource *  mr      = nullptr);

/// Align two signals by zero-padding the leading one.
///
/// Calls `finddelay(x, y, max_lag)` internally, then zero-pads on the
/// left so the returned `xa` and `ya` are time-aligned and have equal
/// length.
///
/// @param x         First signal.
/// @param y         Second signal.
/// @param max_lag   Cap on the search range. `0` ≡ full.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Tuple `(xa, ya)` with `numel(xa) == numel(ya)`.
std::tuple<Value, Value>
alignsignals(const Value &                x,
             const Value &                y,
             long                         max_lag = 0,
             std::pmr::memory_resource *  mr      = nullptr);

} // namespace numkit::signal
