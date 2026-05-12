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
Value dcgain(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Result of `margin(sys)`.
struct MarginResult {
    Value Gm;    ///< linear gain margin (NOT dB; MATLAB convention)
    Value Pm;    ///< phase margin (degrees)
    Value Wcg;   ///< phase crossover (phase = -180°)
    Value Wcp;   ///< gain  crossover (|H| = 1)
};

/// `[Gm, Pm, Wcg, Wcp] = margin(sys)` — gain and phase margins.
/// Any margin that does not exist is returned as Inf (no crossing
/// found on the default frequency grid).
MarginResult margin(const Value &sys,
                    std::pmr::memory_resource *mr = nullptr);

/// `S = stepinfo(sys)` — struct with fields:
///   RiseTime, SettlingTime, SettlingMin, SettlingMax,
///   Overshoot, Undershoot, Peak, PeakTime.
/// All times in seconds. Defaults: 10–90 % rise, 2 % settling band.
Value stepinfo(const Value &sys, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
