// libs/control/include/numkit/control/analyze/analyze.hpp
//
// Steady-state and time-domain quality metrics on top of the
// existing tf/zpk/ss + step/bode primitives.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// `dcgain(sys)` — steady-state gain.
///   continuous : H(0)
///   discrete   : H(1) (i.e. evaluate tf at z = 1)
Value dcgain(std::pmr::memory_resource *mr, const Value &sys);

/// `[Gm, Pm, Wcg, Wcp] = margin(sys)` — gain and phase margins.
///   Gm  : linear gain margin (NOT dB; MATLAB convention).
///   Pm  : phase margin in degrees.
///   Wcg : phase crossover frequency (phase = -180°).
///   Wcp : gain crossover frequency (|H| = 1).
/// Any margin that does not exist is returned as Inf (no crossing
/// found on the default frequency grid).
void margin(std::pmr::memory_resource *mr, const Value &sys,
            Value *Gm, Value *Pm, Value *Wcg, Value *Wcp);

/// `S = stepinfo(sys)` — struct with fields:
///   RiseTime, SettlingTime, SettlingMin, SettlingMax,
///   Overshoot, Undershoot, Peak, PeakTime.
/// All times in seconds. Defaults: 10–90 % rise, 2 % settling band.
Value stepinfo(std::pmr::memory_resource *mr, const Value &sys);

} // namespace numkit::control
