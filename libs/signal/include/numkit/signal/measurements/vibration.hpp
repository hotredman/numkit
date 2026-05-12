// libs/signal/include/numkit/signal/measurements/vibration.hpp
//
// Vibration analysis — envelope spectrum, tachometer→RPM, rainflow
// cycle counting, time-synchronous averaging. The full MATLAB Signal
// Processing Toolbox set (modal*, ordertrack, rpm*maps, etc.) is
// substantial; this header lands the high-frequency-of-use subset.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// envspectrum(x[, fs]) — envelope spectrum: magnitude FFT of the
/// AC-coupled (mean-subtracted) analytic-signal envelope. Returns
/// (Es, F). Without fs, F is on [0, π] (rad/sample); with fs, F is in
/// [0, fs/2] (Hz).
std::tuple<Value, Value>
envspectrum(const Value &x, double fs = 0.0, std::pmr::memory_resource *mr = nullptr);

/// tachorpm(t, fs[, threshold[, ppr]]) — convert a tachometer pulse
/// signal `t` (sampled at `fs`) into an RPM trace. `threshold`
/// (default = midpoint of t's min/max) is the crossing level for
/// pulse detection; `ppr` is pulses per revolution (default 1).
/// Returns (rpm, t_pulse) — rpm[i] is the RPM at the i-th detected
/// pulse, t_pulse[i] the time stamp.
std::tuple<Value, Value>
tachorpm(const Value &x, double fs, const Value &threshold = Value::Empty, int ppr = 1, std::pmr::memory_resource *mr = nullptr);

/// rainflow(x) — ASTM E1049-85 cycle counting on the input signal.
/// Returns a matrix [count, range, mean] with one row per counted
/// cycle (count is 1.0 for full cycles and 0.5 for half cycles).
Value rainflow(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// tsa(x, fs, rpm, fs_rpm[, n_per_rev]) — time-synchronous average:
/// resample x to angular domain using the RPM time series and average
/// across revolutions. Returns (avg, theta) where avg is one
/// revolution's worth of samples and theta is the angle vector
/// [0, 2π) with `n_per_rev` samples (default 1024).
std::tuple<Value, Value>
tsa(const Value &x, double fs, const Value &rpm, double fs_rpm, int n_per_rev = 1024, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
