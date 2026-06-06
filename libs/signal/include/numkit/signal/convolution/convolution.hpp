// libs/signal/include/numkit/signal/convolution/convolution.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>
#include <tuple>

namespace numkit::signal {

/// 1-D convolution of two real vectors.
///
/// Computes \f$ c[k] = \sum_{j} a[j] \cdot b[k - j] \f$ (the standard
/// non-cyclic convolution). Switches between direct O(na·nb) and
/// FFT-based O((na+nb)·log(na+nb)) implementations based on input size.
///
/// @param a      First input vector (real, any length).
/// @param b      Second input vector (real, any length).
/// @param shape  Output trimming mode:
///                 - `"full"`  (default): full convolution, length na + nb - 1.
///                 - `"same"`:  central na samples (or max of na, nb), aligned to `a`.
///                 - `"valid"`: only the region where the two inputs overlap fully,
///                              length `|na - nb| + 1`.
/// @param mr     Memory resource (nullptr → process default).
/// @return       1-D DOUBLE vector with the shape per the `shape` flag.
/// @throws       numkit::Error  on unknown `shape` keyword.
///
/// @code
/// Value c = conv({1, 2, 3}, {1, 1, 1});            // full: {1, 3, 6, 5, 3}
/// Value s = conv({1, 2, 3, 4, 5}, {1, 1, 1}, "same"); // {3, 6, 9, 12, 9}
/// @endcode
///
/// @see conv2, convn, deconv, cconv
Value conv(const Value &                a,
           const Value &                b,
           const std::string &          shape = "full",
           std::pmr::memory_resource *  mr    = nullptr);

/// Polynomial long-division: `b = conv(a, q) + r`.
///
/// Recovers the quotient `q` and remainder `r` such that the original
/// polynomial relationship holds (the `[q, r] = deconv(b, a)` form).
///
/// @param b   Dividend polynomial (real row/column vector).
/// @param a   Divisor polynomial.  `a[0]` must be non-zero.
///            `length(a)` must be ≤ `length(b)`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(q, r)` with `length(q) = length(b) - length(a) + 1`
///            and `length(r) = length(b)`.
/// @throws    numkit::Error  if `a` is longer than `b`, or `a[0] == 0`.
///
/// @see conv
std::tuple<Value, Value>
deconv(const Value &                b,
       const Value &                a,
       std::pmr::memory_resource *  mr = nullptr);

/// Cross-correlation of two real vectors.
///
/// Returns `(c, lags)` where
/// \f$ c[k] = \sum_n x[n] \cdot y[n - k] \f$
/// for k ∈ `[-(maxN-1), maxN-1]` (maxN = max(nx, ny)). The lag vector
/// `lags` is integer-valued and aligns one-to-one with `c`.
///
/// `c` has length nx + ny - 1.
///
/// @param x   First signal (real, row or column).
/// @param y   Second signal (real, row or column).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(c, lags)`, both column vectors.
///
/// @code
/// auto [c, lags] = xcorr({1, 2, 3, 4}, {1, 0, 0, 1});
/// // c has 7 elements, lags = -3..3
/// @endcode
///
/// @see xcov, finddelay
std::tuple<Value, Value>
xcorr(const Value &                x,
      const Value &                y,
      std::pmr::memory_resource *  mr = nullptr);

/// @brief Auto-correlation — equivalent to `xcorr(x, x)`.
///
/// @param x   Signal (real, row or column).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(c, lags)` — same shape as the two-arg form;
///            `c` has length `2 · numel(x) - 1`.
/// @see xcorr(const Value &, const Value &, std::pmr::memory_resource *)
inline std::tuple<Value, Value>
xcorr(const Value &x, std::pmr::memory_resource *mr = nullptr)
{
    return xcorr(x, x, mr);
}

/// Cross-covariance of two real vectors.
///
/// Computes `xcorr(x - mean(x), y - mean(y))` with MATLAB's scaling and
/// lag handling. The mean is subtracted before correlation, so the
/// result is invariant to DC offsets.
///
/// @param x         First signal (real, row or column).
/// @param y         Second signal (real, row or column).
/// @param maxlag    Maximum lag; `< 0` selects the full range `N-1`
///                  (`N = max(numel(x), numel(y))`). Result is cropped
///                  (or zero-padded) to lags `-maxlag..maxlag`.
/// @param scaleopt  `"none"` (default), `"biased"`, `"unbiased"`, or
///                  `"coeff"` (a.k.a. `"normalized"`).
/// @param mr        Memory resource (nullptr → process default).
/// @return          Tuple `(c, lags)`.
///
/// @see xcorr
std::tuple<Value, Value>
xcov(const Value &                x,
     const Value &                y,
     int                          maxlag,
     const std::string &          scaleopt,
     std::pmr::memory_resource *  mr = nullptr);

/// @brief Cross-covariance with full lags and `"none"` scaling.
inline std::tuple<Value, Value>
xcov(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr)
{
    return xcov(x, y, -1, std::string("none"), mr);
}

/// @brief Auto-covariance — equivalent to `xcov(x, x)`.
///
/// @param x   Signal (real, row or column).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(c, lags)` — DC-removed auto-correlation; same
///            shape as the two-arg form.
/// @see xcov(const Value &, const Value &, std::pmr::memory_resource *)
inline std::tuple<Value, Value>
xcov(const Value &x, std::pmr::memory_resource *mr = nullptr)
{
    return xcov(x, x, mr);
}

/// 2-D convolution of two real matrices.
///
/// Computes
/// \f$ C[i,j] = \sum_{m,n} A[m,n] \cdot B[i-m, j-n] \f$
/// with zero-padding outside the input range.
///
/// @param A      First matrix.
/// @param B      Second matrix.
/// @param shape  Output trimming:
///                 - `"full"`  (default): (rA + rB - 1) × (cA + cB - 1).
///                 - `"same"`:  size of A, centered on the full result.
///                 - `"valid"`: only the fully-overlapping region.
/// @param mr     Memory resource (nullptr → process default).
/// @return       DOUBLE matrix with the shape per the `shape` flag.
/// @throws       numkit::Error  on unknown `shape`.
///
/// @note Direct nested-loop implementation; for large inputs FFT-based
///       2-D convolution would be faster but is not yet wired in.
///
/// @see conv, convn, filter2
Value conv2(const Value &                A,
            const Value &                B,
            const std::string &          shape = "full",
            std::pmr::memory_resource *  mr    = nullptr);

/// 2-D filter — `filter2(h, X[, shape])`.
///
/// Equivalent to `conv2(X, rot90(h, 2), shape)`: the filter kernel `h`
/// is rotated 180° before convolution, matching the engineering
/// convention used in image filtering.
///
/// @param h      Filter kernel (2-D matrix).
/// @param X      Input image (2-D matrix).
/// @param shape  Output trimming, see `conv2`. Default `"same"`.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Filtered DOUBLE matrix.
///
/// @see conv2
Value filter2(const Value &                h,
              const Value &                X,
              const std::string &          shape = "same",
              std::pmr::memory_resource *  mr    = nullptr);

/// N-D convolution.
///
/// Generalisation of `conv` / `conv2` to N dimensions. Currently
/// supports 1-D, 2-D, and 3-D inputs.
///
/// @param A      First N-D array.
/// @param B      Second N-D array.
/// @param shape  Output trimming (`"full"` / `"same"` / `"valid"`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       DOUBLE array with shape per `shape`.
/// @throws       numkit::Error  on unknown `shape` or unsupported rank.
///
/// @see conv, conv2
Value convn(const Value &                A,
            const Value &                B,
            const std::string &          shape = "full",
            std::pmr::memory_resource *  mr    = nullptr);

} // namespace numkit::signal
