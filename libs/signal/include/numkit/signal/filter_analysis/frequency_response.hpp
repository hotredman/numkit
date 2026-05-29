// libs/signal/include/numkit/signal/filter_analysis/frequency_response.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Frequency response of a digital filter.
///
/// Computes
/// \f$ H(e^{j\omega}) = \frac{B(e^{j\omega})}{A(e^{j\omega})} \f$
/// on a uniform grid \f$ \omega \in [0, \pi] \f$.
///
/// @param b     Numerator polynomial (real row / column vector).
/// @param a     Denominator polynomial. For FIR, pass `{1.0}`.
///              `a[0]` must be non-zero.
/// @param npts  Number of frequency points. Default 512.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Tuple `(H, W)`:
///                - `H` — `npts × 1` COMPLEX vector of response values.
///                - `W` — `npts × 1` DOUBLE vector of frequencies in rad/sample.
///
/// @code
/// auto [b, a] = butter(4, 0.3);
/// auto [H, w] = freqz(b, a, 1024);
/// // magnitude in dB:
/// // mag_dB = 20*log10(abs(H))
/// @endcode
///
/// @see phasez, grpdelay, freqs
///
/// `whole` selects the full unit circle: the grid spans `[0, 2π)` with
/// `w = 2π·(0:n-1)/n` (MATLAB `freqz(..., 'whole')`). Default is the
/// half circle `[0, π)`, `w = π·(0:n-1)/n`.
std::tuple<Value, Value>
freqz(const Value &                b,
      const Value &                a,
      size_t                       npts = 512,
      std::pmr::memory_resource *  mr   = nullptr,
      bool                         whole = false);

/// Unwrapped phase response of a digital filter.
///
/// Equivalent to `unwrap(angle(freqz(b, a, npts)))`.
///
/// @param b     Numerator polynomial.
/// @param a     Denominator polynomial.
/// @param npts  Number of frequency points. Default 512.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Tuple `(phi, W)` — phase in radians and frequencies.
///
/// @see freqz, grpdelay
std::tuple<Value, Value>
phasez(const Value &                b,
       const Value &                a,
       size_t                       npts = 512,
       std::pmr::memory_resource *  mr   = nullptr);

/// Group delay of a digital filter.
///
/// Computes the negative derivative of the unwrapped phase response
/// with respect to ω, i.e. \f$ \tau_g(\omega) = -\frac{d\phi}{d\omega} \f$.
/// Implemented via finite-differences of `unwrap(angle(freqz(…)))` on
/// the uniform grid; endpoint values use one-sided differences.
///
/// @param b     Numerator polynomial.
/// @param a     Denominator polynomial.
/// @param npts  Number of frequency points. Default 512.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Tuple `(gd, W)` — group delay in samples per radian
///              and frequency grid in rad/sample.
///
/// @see freqz, phasez, phasedelay
std::tuple<Value, Value>
grpdelay(const Value &                b,
         const Value &                a,
         size_t                       npts = 512,
         std::pmr::memory_resource *  mr   = nullptr);

} // namespace numkit::signal
