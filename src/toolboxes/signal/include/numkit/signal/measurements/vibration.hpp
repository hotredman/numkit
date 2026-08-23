/// @file vibration.hpp
/// @ingroup group_signal
// toolboxes/signal/include/numkit/signal/measurements/vibration.hpp
//
// Vibration analysis — envelope spectrum, tachometer → RPM, rainflow
// cycle counting, time-synchronous averaging.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::signal {

/// @addtogroup group_signal
/// @{


/// Envelope spectrum: magnitude FFT of the AC-coupled analytic-signal envelope.
///
/// Pipeline: `|hilbert(x - mean(x))| → fft → |·|`. Used in vibration
/// diagnostics to extract modulation frequencies (e.g. bearing fault
/// signatures) from a high-frequency carrier.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `0.0` (default) → frequencies returned
///            in rad/sample (`[0, π]`). With `fs > 0` → Hz (`[0, fs/2]`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(Es, F)` — magnitude spectrum and frequency grid,
///            both single-sided column vectors of length `floor(N/2)+1`.
///
/// @see hilbert
std::tuple<Value, Value>
envspectrum(const Value &                x,
            double                       fs = 0.0,
            std::pmr::memory_resource *  mr = nullptr);

/// Convert a tachometer pulse signal to an RPM trace.
///
/// Detects rising-edge crossings of `threshold`, then computes RPM from
/// inter-pulse intervals: `rpm = 60 / (Δt · ppr)`.
///
/// @param x          Tachometer pulse signal (real 1-D).
/// @param fs         Sample rate of `x` in Hz.
/// @param threshold  Crossing level. `Value::Empty` (default) → midpoint
///                   of `min(x)` and `max(x)`.
/// @param ppr        Pulses per revolution. Default 1.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(rpm, t_pulse)` — RPM samples and their
///                   timestamps in seconds.
///
/// @see tsa
std::tuple<Value, Value>
tachorpm(const Value &                x,
         double                       fs,
         const Value &                threshold = Value::Empty,
         int                          ppr       = 1,
         std::pmr::memory_resource *  mr        = nullptr);

/// Rainflow cycle counting (ASTM E1049-85).
///
/// Standard counting method for fatigue analysis. Identifies full and
/// half cycles in a load-history signal, classifying each by amplitude
/// range and mean.
///
/// @param x   Real 1-D load history.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `Nrows × 5` matrix where each row is
///            `[count, range, mean, start_idx, end_idx]`.
///            `count = 1.0` for full cycles, `0.5` for half cycles.
Value rainflow(const Value &                x,
               std::pmr::memory_resource *  mr = nullptr);

/// Time-synchronous average (TSA).
///
/// Resamples `x` to the angular domain using the `rpm` time series, then
/// averages across complete revolutions. Reveals periodic features that
/// are coherent with shaft rotation (e.g. gear-mesh tones).
///
/// @param x           Vibration signal (real 1-D).
/// @param fs          Sample rate of `x` in Hz.
/// @param rpm         RPM trace (real 1-D, sampled at `fs_rpm`).
/// @param fs_rpm      Sample rate of `rpm` in Hz.
/// @param n_per_rev   Samples per revolution in the output. Default 1024.
/// @param mr          Memory resource (nullptr → process default).
/// @return            Tuple `(avg, theta)` — averaged signal over one
///                    revolution and angle vector
///                    `[0, 2π · (n_per_rev − 1) / n_per_rev]`.
///
/// @see tachorpm, envspectrum
std::tuple<Value, Value>
tsa(const Value &                x,
    double                       fs,
    const Value &                rpm,
    double                       fs_rpm,
    int                          n_per_rev = 1024,
    std::pmr::memory_resource *  mr        = nullptr);


/// @}
} // namespace numkit::signal
