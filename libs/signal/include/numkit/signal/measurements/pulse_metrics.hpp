// libs/signal/include/numkit/signal/measurements/pulse_metrics.hpp
//
// Pulse and transition metrics for digital / step-shaped time-series.
// MATLAB Signal Processing Toolbox `risetime / falltime / overshoot /
// undershoot / settlingtime / dutycycle / midcross / pulsewidth /
// pulseperiod / pulsesep / slewrate / statelevels` family.
//
// Conventions:
//   * `x` is a real, sampled time-series (1-D or vector along first dim).
//   * `fs` is the sample rate in Hz; pass nullptr to default to fs=1
//     (samples become the unit). When supplied, returned times are in
//     seconds.
//   * Reference levels are computed from `statelevels(x)` (histogram
//     mode of two halves) unless explicit levels are supplied. The
//     state boundaries are the canonical MATLAB defaults: 10% / 90%
//     for rise/fall, 50% for mid-state, with a 2% tolerance for
//     "settled".
//   * Outputs are scalars when only one transition / pulse is found,
//     vectors when many.
//
// `statelevels` is the foundation; all of the rest delegate to it
// (or accept user-supplied levels via a 2-arg form not exposed yet).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// statelevels(x) — return [low, high] state levels via histogram-mode
/// estimate (MATLAB default: 100 bins, split at mean(x)). Returns a
/// 1×2 row vector. For input that is too flat (single bin populated)
/// returns [min, max].
Value statelevels(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// midcross(x[, fs]) — time stamps where x crosses the 50% mid-state
/// reference level (linearly interpolated). Returns a column vector
/// (length depends on the data). With fs=NULL, times are sample
/// indices (1-based, fractional).
Value midcross(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// risetime(x[, fs]) — duration of the lower-to-upper state boundary
/// transitions (10% → 90% by default). Returns a column vector with
/// one entry per detected positive-going transition; empty if none.
Value risetime(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// falltime(x[, fs]) — duration of upper-to-lower state boundary
/// transitions (90% → 10%). Returns column vector; empty if none.
Value falltime(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// slewrate(x[, fs]) — slope of each transition: (upper - lower) /
/// transition_duration, sign matches direction. Column vector.
Value slewrate(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// overshoot(x[, fs]) — percent overshoot above the upper state level
/// for each positive-going transition. Returned as a column vector of
/// percentages (e.g. 5.0 means 5% above the upper level).
Value overshoot(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// undershoot(x[, fs]) — percent undershoot below the lower state
/// level. Column vector.
Value undershoot(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// settlingtime(x[, fs, tol]) — time from the start of each transition
/// until x stays within `tol` (default 2%) of the destination state.
/// Column vector.
Value settlingtime(const Value &x, const Value &fs = Value::Empty, double tol = 0.02, std::pmr::memory_resource *mr = nullptr);

/// pulsewidth(x[, fs]) — duration each pulse stays above the mid-state.
/// Column vector with one entry per pulse.
Value pulsewidth(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// pulseperiod(x[, fs]) — period between consecutive same-direction
/// crossings of the mid-state. Column vector.
Value pulseperiod(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// pulsesep(x[, fs]) — separation between consecutive pulses (the
/// time spent below the mid-state between two pulses). Column vector.
Value pulsesep(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// dutycycle(x[, fs]) — fraction (0..1) of each pulse period spent
/// above the mid-state. Column vector with one entry per period.
Value dutycycle(const Value &x, const Value &fs = Value::Empty, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
