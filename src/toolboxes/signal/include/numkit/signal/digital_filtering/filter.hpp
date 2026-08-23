/// @file filter.hpp
/// @ingroup group_signal
// toolboxes/signal/include/numkit/signal/digital_filtering/filter.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// Direct-form II transposed IIR / FIR filter.
///
/// Implements the standard difference equation
/// \f[
///   a_0\, y[n] = \sum_{k=0}^{M} b_k\, x[n-k] - \sum_{k=1}^{N} a_k\, y[n-k]
/// \f]
/// Normalisation by `a[0]` is folded into the coefficients up-front, so
/// the runtime per-sample cost is one MAC per `b` and `a` coefficient.
///
/// @param b   Numerator polynomial. Must be non-empty.
/// @param a   Denominator polynomial. `a[0]` must be non-zero.
/// @param x   Input signal (real vector or matrix).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Filtered signal, same shape as `x`.
///
/// @throws    numkit::Error  if `a[0] == 0` or either polynomial is empty.
///
/// @code
/// auto [b, a] = butter(4, 0.3);
/// Value y = filter(b, a, x);
/// @endcode
///
/// @see filtfilt, sosfilt
Value filter(const Value &                b,
             const Value &                a,
             const Value &                x,
             std::pmr::memory_resource *  mr = nullptr);

/// Zero-phase forward-backward filtering.
///
/// Applies `filter(b, a, ·)` twice: once forward, once on the reversed
/// signal, then reverses again. Cancels the filter's phase response so
/// the output has zero phase distortion (but doubles the effective
/// magnitude response).
///
/// Uses edge-reflected padding (length `3 · max(nb, na)`) to suppress
/// startup transients at both ends.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial. `a[0] != 0`.
/// @param x   Input signal. Length must exceed `3 · max(nb, na)`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Zero-phase filtered signal, same shape as `x`.
///
/// @throws    numkit::Error  if `a[0] == 0` or `x` is too short.
///
/// @see filter
Value filtfilt(const Value &                b,
               const Value &                a,
               const Value &                x,
               std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
