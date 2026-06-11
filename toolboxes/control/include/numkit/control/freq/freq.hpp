// toolboxes/control/include/numkit/control/freq/freq.hpp
//
// Frequency-domain responses for tf / zpk / ss systems.
// Implementation reduces every input form to a (num, den) coefficient
// pair and evaluates the rational at s = jω (continuous) or
// z = exp(jω·Ts) (discrete) directly.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <utility>

namespace numkit::control {

/// Single-point frequency response (`H = evalfr(sys, f)`).
///
/// Returns the complex value of the transfer function at a single
/// frequency `f` (rad/s):
///   - continuous: @f$ H(j f) @f$.
///   - discrete:   @f$ H\!\left(e^{j f T_s}\right) @f$.
///
/// @param sys  LTI struct (tf / zpk / ss).
/// @param f    Frequency in rad/s.
/// @param mr   Memory resource (nullptr → process default).
/// @return     1×1 complex Value.
///
/// @see freqresp, bode
Value evalfr(const Value &sys, double f,
             std::pmr::memory_resource *mr = nullptr);

/// Frequency response on a user-supplied grid (`H = freqresp(sys, w)`).
///
/// @param sys  LTI struct (tf / zpk / ss).
/// @param w    Column or row vector of frequencies (rad/s).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Column vector of complex H values, one per element of `w`.
///
/// @see evalfr, bode, nyquist
Value freqresp(const Value &sys, const Value &w,
               std::pmr::memory_resource *mr = nullptr);

/// Result of @ref bode "bode(sys, w)".
struct BodeResult {
    Value mag;     ///< Linear magnitude |H(jω)|, column vector.
    Value phase;   ///< Phase in degrees, column vector (continuously unwrapped).
    Value w;       ///< Frequencies used (rad/s).
};

/// Bode magnitude and phase response (`[mag, phase, w] = bode(sys, w)`).
///
/// Evaluates the response on `wArg` (or a default logarithmic grid
/// covering the dominant pole/zero magnitudes if `wArg` is empty).
/// Phase is unwrapped sample-by-sample so it doesn't jump by 360°.
///
/// @param sys   LTI struct.
/// @param wArg  Frequency vector (rad/s) or empty to auto-pick.
/// @param mr    Memory resource (nullptr → process default).
/// @return      @ref BodeResult; bind via `auto b = bode(sys, w);`.
///
/// @code
/// auto b = bode(plant, Value::Empty);
/// // b.mag, b.phase, b.w are column vectors of equal length.
/// @endcode
///
/// @see margin, nyquist, freqresp
BodeResult bode(const Value &sys, const Value &wArg,
                std::pmr::memory_resource *mr = nullptr);

/// Result of @ref nyquist "nyquist(sys, w)".
struct NyquistResult {
    Value re;   ///< Real part of H(jω).
    Value im;   ///< Imaginary part of H(jω).
    Value w;    ///< Frequencies used (rad/s).
};

/// Nyquist plot data (`[re, im, w] = nyquist(sys, w)`).
///
/// Same grid logic as @ref bode but returns the Cartesian components
/// of H instead of magnitude / phase.
///
/// @param sys   LTI struct.
/// @param wArg  Frequency vector (rad/s) or empty to auto-pick.
/// @param mr    Memory resource (nullptr → process default).
/// @return      @ref NyquistResult.
///
/// @see bode, freqresp
NyquistResult nyquist(const Value &sys, const Value &wArg,
                      std::pmr::memory_resource *mr = nullptr);

/// Root locus (`[r, k] = rlocus(sys, k)`).
///
/// For each gain @f$ k @f$ in `kVec`, compute the n closed-loop poles of
/// the negative-feedback system @f$ T = G / (1 + k\,G) @f$, i.e. roots
/// of `den(s) + k · num(s) = 0`.
///
/// If `kVec` is empty, a default sweep is built: 0 plus 100 log-spaced
/// points from 1e-2 to 1e3 (101 gains total).
///
/// @param sys   LTI struct.
/// @param kVec  Gain vector or empty for the default sweep.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `(r, k)` where
///                - r : length(k) × n complex matrix of closed-loop poles.
///                - k : the gain vector actually used.
///              Bind via `auto [r, k] = rlocus(sys, kVec);`.
///
/// @see margin, bode
std::pair<Value, Value>
rlocus(const Value &sys, const Value &kVec,
       std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
