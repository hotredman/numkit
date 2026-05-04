// libs/control/include/numkit/control/freq/freq.hpp
//
// Frequency-domain responses for tf / zpk / ss systems.
// Implementation reduces every input form to a (num, den) coefficient
// pair and evaluates the rational at s = jω (continuous) or
// z = exp(jω·Ts) (discrete) directly.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// `H = evalfr(sys, f)` — single complex H(jω) (continuous) or H(z)
/// (discrete) at frequency `f` (rad/s).
Value evalfr(std::pmr::memory_resource *mr,
             const Value &sys, double f);

/// `H = freqresp(sys, w)` — column vector of complex H values at each
/// ω in `w`.
Value freqresp(std::pmr::memory_resource *mr,
               const Value &sys, const Value &w);

/// `[mag, phase, w] = bode(sys [, w])` — magnitude (linear), phase (deg).
/// `w` defaults to a logarithmic grid covering the dominant poles.
void bode(std::pmr::memory_resource *mr,
          const Value &sys, const Value &wArg,
          Value *magOut, Value *phaseOut, Value *wOut);

/// `[re, im, w] = nyquist(sys [, w])` — real and imaginary parts of H.
void nyquist(std::pmr::memory_resource *mr,
             const Value &sys, const Value &wArg,
             Value *reOut, Value *imOut, Value *wOut);

} // namespace numkit::control
