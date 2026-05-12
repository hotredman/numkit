// libs/signal/include/numkit/signal/filter_analysis/responses.hpp
//
// Time- and phase-domain response helpers around the (b, a) digital-
// filter representation. Built on the existing filter() / freqz()
// kernels — no new SIMD here.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Impulse response of a digital filter.
///
/// Computes the first `n` samples of `h[k] = filter(b, a, δ[k])`, where
/// `δ` is the unit impulse.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial. Pass `{1.0}` for FIR.
/// @param n   Number of output samples. `0` (default) → use `impzlength`
///            to pick a reasonable default.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(h, t)` — impulse response samples and zero-based
///            sample indices (DOUBLE column vectors of length `n`).
///
/// @code
/// auto [b, a] = butter(4, 0.3);
/// auto [h, t] = impz(b, a, 128);
/// @endcode
///
/// @see impzlength, stepz
std::tuple<Value, Value>
impz(const Value &                b,
     const Value &                a,
     size_t                       n  = 0,
     std::pmr::memory_resource *  mr = nullptr);

/// Heuristic estimate of the number of significant impulse-response samples.
///
/// FIR filters (a is scalar 1) → returns `numel(b)`.
/// IIR filters → returns
/// `max(50, ceil(-log(1e-5) / log(max_pole_radius)))`, clipped to [50, 8192].
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Suggested impulse-response length.
///
/// @see impz
size_t impzlength(const Value &                b,
                  const Value &                a,
                  std::pmr::memory_resource *  mr = nullptr);

/// Step response of a digital filter.
///
/// Computes `s[k] = filter(b, a, ones(n, 1))` — the response to a unit
/// step input.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param n   Number of output samples. `0` (default) → `impzlength`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(s, t)` — step response and sample indices.
///
/// @see impz
std::tuple<Value, Value>
stepz(const Value &                b,
      const Value &                a,
      size_t                       n  = 0,
      std::pmr::memory_resource *  mr = nullptr);

/// Phase delay of a digital filter.
///
/// Computes \f$ \tau_p(\omega) = -\phi(\omega) / \omega \f$ for
/// ω ∈ [0, π]. Phase is unwrapped before division; the ω=0 sample uses
/// the next-bin value to avoid 0/0.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param n   Number of frequency points. Default 512.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(pd, w)` — phase delay (samples) and frequency grid.
///
/// @see grpdelay, phasez
std::tuple<Value, Value>
phasedelay(const Value &                b,
           const Value &                a,
           size_t                       n  = 512,
           std::pmr::memory_resource *  mr = nullptr);

/// Equivalent zero-phase response.
///
/// Removes the linear-phase delay component, returning the amplitude
/// function `Hr(ω)` of the filter. For symmetric or antisymmetric FIR
/// filters this is a real-valued function that may be negative
/// (sign flip ↔ phase jump of π in the original `H(e^{jω})`).
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param n   Number of frequency points. Default 512.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(Hr, w)` — amplitude function (real-valued for
///            linear-phase FIR) and frequency grid in [0, π].
///
/// @see freqz, phasez
std::tuple<Value, Value>
zerophase(const Value &                b,
          const Value &                a,
          size_t                       n  = 512,
          std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
