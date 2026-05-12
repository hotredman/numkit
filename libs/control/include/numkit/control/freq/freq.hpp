// libs/control/include/numkit/control/freq/freq.hpp
//
// Frequency-domain responses for tf / zpk / ss systems.
// Implementation reduces every input form to a (num, den) coefficient
// pair and evaluates the rational at s = jω (continuous) or
// z = exp(jω·Ts) (discrete) directly.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <utility>

namespace numkit::control {

/// `H = evalfr(sys, f)` — single complex H(jω) (continuous) or H(z)
/// (discrete) at frequency `f` (rad/s).
Value evalfr(const Value &sys, double f,
             std::pmr::memory_resource *mr = nullptr);

/// `H = freqresp(sys, w)` — column vector of complex H values at each
/// ω in `w`.
Value freqresp(const Value &sys, const Value &w,
               std::pmr::memory_resource *mr = nullptr);

/// Result of `bode(sys [, w])`.
struct BodeResult {
    Value mag;     ///< magnitude (linear), column vector
    Value phase;   ///< phase (deg),         column vector
    Value w;       ///< frequencies used (rad/s)
};

/// `[mag, phase, w] = bode(sys [, w])` — magnitude (linear), phase (deg).
/// `w` defaults to a logarithmic grid covering the dominant poles.
BodeResult bode(const Value &sys, const Value &wArg,
                std::pmr::memory_resource *mr = nullptr);

/// Result of `nyquist(sys [, w])`.
struct NyquistResult {
    Value re;
    Value im;
    Value w;
};

/// `[re, im, w] = nyquist(sys [, w])` — real and imaginary parts of H.
NyquistResult nyquist(const Value &sys, const Value &wArg,
                      std::pmr::memory_resource *mr = nullptr);

/// `[r, k] = rlocus(sys [, kVec])` — root locus.
/// For each gain k in `kVec`, compute the n closed-loop poles of
/// the negative-feedback system  T = G / (1 + k·G), i.e. roots of
/// `den(s) + k · num(s) = 0`. Returns
///   r : (length(k)) × n complex matrix, one row per gain
///   k : the gain vector actually used
/// If `kVec` is empty, a default logarithmic sweep is generated
/// covering 0 plus 100 points from 1e-2 to 1e3.
std::pair<Value, Value>
rlocus(const Value &sys, const Value &kVec,
       std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
