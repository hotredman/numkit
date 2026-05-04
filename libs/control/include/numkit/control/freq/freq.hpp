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

/// `[r, k] = rlocus(sys [, kVec])` — root locus.
/// For each gain k in `kVec`, compute the n closed-loop poles of
/// the negative-feedback system  T = G / (1 + k·G), i.e. roots of
/// `den(s) + k · num(s) = 0`. Returns
///   r : (length(k)) × n complex matrix, one row per gain
///   k : the gain vector actually used
/// If `kVec` is empty, a default logarithmic sweep is generated
/// covering 0 plus 100 points from 1e-2 to 1e3.
void rlocus(std::pmr::memory_resource *mr,
            const Value &sys, const Value &kVec,
            Value *rOut, Value *kOut);

} // namespace numkit::control
