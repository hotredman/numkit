// toolboxes/control/include/numkit/control/analyze/analyze.hpp
//
// Steady-state and time-domain quality metrics on top of the
// existing tf/zpk/ss + step/bode primitives.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::control {

/// DC (steady-state) gain of an LTI system.
///
/// Evaluates the transfer function at the steady-state frequency:
///   - continuous (Ts == 0): @f$ H(0) @f$
///   - discrete (Ts > 0):    @f$ H(1) @f$ (i.e. tf evaluated at z = 1)
///
/// @param sys  LTI struct (tf / zpk / ss as built by toolboxes/control/lti).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Real scalar Value (or complex if the model is complex).
///
/// @code
/// Value sys = tf({1}, {1, 2, 1});     // 1 / (s² + 2s + 1)
/// Value g   = dcgain(sys);             // → 1.0
/// @endcode
///
/// @see margin, stepinfo, evalfr
Value dcgain(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Result of @ref margin "margin(sys)" — gain / phase margin pair plus
/// the frequencies at which they occur.
///
/// Any margin that does not exist on the default frequency grid is
/// returned as +Inf (no crossover detected).
struct MarginResult {
    Value Gm;    ///< Gain margin (linear, **not** dB).
    Value Pm;    ///< Phase margin in degrees.
    Value Wcg;   ///< Phase-crossover frequency (where phase = −180°), rad/s.
    Value Wcp;   ///< Gain-crossover frequency (where |H| = 1), rad/s.
};

/// Gain and phase margins (`[Gm, Pm, Wcg, Wcp] = margin(sys)`).
///
/// Builds a dense Bode grid via @ref bode "bode(sys)" and scans for
/// the first crossings of phase = −180° (gain margin) and |H| = 1
/// (phase margin) from low to high frequency. If a crossing isn't
/// found the corresponding margin is +Inf.
///
/// @param sys  LTI struct (tf / zpk / ss).
/// @param mr   Memory resource (nullptr → process default).
/// @return     @ref MarginResult — bind via `auto m = margin(sys);`.
///
/// @code
/// auto m = margin(plant);
/// fprintf("GM = %.2f at ω = %.2f, PM = %.2f at ω = %.2f\n",
///         m.Gm.toScalar(), m.Wcg.toScalar(),
///         m.Pm.toScalar(), m.Wcp.toScalar());
/// @endcode
///
/// @see bode, dcgain, stepinfo
MarginResult margin(const Value &sys,
                    std::pmr::memory_resource *mr = nullptr);

/// Step-response quality metrics (`S = stepinfo(sys)`).
///
/// Runs the system through @ref step_response with a 2× extended
/// horizon and returns a struct with the fields:
///
///   - `RiseTime`    — 10 % → 90 % rise interval (s).
///   - `SettlingTime`— last time |y − yfinal| exceeds the 2 % band (s).
///   - `SettlingMin` — minimum of y inside the settling band.
///   - `SettlingMax` — maximum of y inside the settling band.
///   - `Overshoot`   — peak overshoot as a percentage of yfinal.
///   - `Undershoot`  — peak undershoot as a percentage of yfinal.
///   - `Peak`        — absolute peak value.
///   - `PeakTime`    — time at which the peak occurs (s).
///
/// All times are in seconds. Defaults: 10–90 % rise band, 2 %
/// settling band.
///
/// @param sys  LTI struct (tf / zpk / ss).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Struct Value with the 8 fields above.
///
/// @see margin, dcgain, step_response
Value stepinfo(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// H-infinity norm of a continuous-time LTI system (`g = hinfnorm(sys)`).
///
/// Returns @f$ \|G\|_\infty = \sup_\omega \sigma_{\max}(G(j\omega)) @f$ via
/// the Bruinsma–Steinbuch Hamiltonian test with bisection on @f$ \gamma @f$
/// (@f$ \gamma @f$ is an upper bound iff the Hamiltonian @f$ M(\gamma) @f$
/// has no purely imaginary eigenvalue). Returns `Inf` for an unstable or
/// marginally stable system (a pole on/right of the jω axis).
///
/// @param sys  Continuous-time LTI struct (ss / tf / zpk).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Scalar `‖G‖∞` (or `Inf`).
/// @throws     Error on a non-LTI input or a discrete-time system
///             (discrete H∞ is not yet supported).
/// @see margin, sigma, dcgain
Value hinfnorm(const Value &sys, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
