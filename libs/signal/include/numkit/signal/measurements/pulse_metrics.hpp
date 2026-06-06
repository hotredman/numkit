// libs/signal/include/numkit/signal/measurements/pulse_metrics.hpp
//
// Pulse and transition metrics for digital / step-shaped time-series.
// The pulse-metric family: `risetime / falltime / overshoot /
// undershoot / settlingtime / dutycycle / midcross / pulsewidth /
// pulseperiod / pulsesep / slewrate / statelevels` family.
//
// Conventions:
//   * `x` is a real, sampled time-series (1-D vector).
//   * `fs` is the sample rate in Hz. `Value::Empty` (default) → fs = 1,
//     so samples are the time unit. When supplied, returned times are in
//     seconds.
//   * Reference levels come from `statelevels(x)` unless explicit levels
//     are supplied. State boundaries use the canonical defaults:
//     10% / 90% for rise / fall, 50% for mid-state, 2% for "settled".
//   * Outputs are column vectors; empty when no transition / pulse found.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// Estimate the [low, high] state levels of a step-shaped signal.
///
/// Uses a histogram-mode estimator: 100 bins, split at `mean(x)`. The
/// mode bin in each half gives the corresponding state level. For inputs
/// that are too flat for the histogram to separate, returns `[min, max]`.
///
/// @param x   Real 1-D signal.
/// @param mr  Memory resource (nullptr → process default).
/// @return    1 × 2 DOUBLE row vector `[low, high]`.
///
/// @see risetime, falltime, midcross
Value statelevels(const Value &                x,
                  std::pmr::memory_resource *  mr = nullptr);

/// Mid-state crossing times.
///
/// Returns the (fractional) times at which `x` crosses the 50%
/// mid-state level, linearly interpolated between adjacent samples.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → fs = 1, output is
///            1-based fractional sample index.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of crossing times. Empty if none.
///
/// @see statelevels
Value midcross(const Value &                x,
               const Value &                fs = Value::Empty,
               std::pmr::memory_resource *  mr = nullptr);

/// Rise time of positive-going transitions.
///
/// Duration spent crossing from the 10% to the 90% state-boundary
/// during each positive-going transition.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → samples.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of rise times; one per detected transition.
///
/// @see falltime, slewrate
Value risetime(const Value &                x,
               const Value &                fs = Value::Empty,
               std::pmr::memory_resource *  mr = nullptr);

/// @brief Fall time of negative-going transitions (90% → 10%).
///
/// Mirror of @ref risetime for downward transitions.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → samples.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of fall times; one per detected transition.
/// @see risetime, slewrate
Value falltime(const Value &x, const Value &fs = Value::Empty,
               std::pmr::memory_resource *mr = nullptr);

/// Slew rate of each transition.
///
/// Computed as `(upper - lower) / transition_duration`; sign matches
/// direction (positive for rising, negative for falling).
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → samples.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector, one entry per transition.
///
/// @see risetime, falltime
Value slewrate(const Value &                x,
               const Value &                fs = Value::Empty,
               std::pmr::memory_resource *  mr = nullptr);

/// @brief Percent overshoot above the upper state.
///
/// One entry per positive-going transition. `5.0` ≡ 5% above the
/// upper state level.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → samples.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of percentages, one per transition.
/// @see undershoot, settlingtime
Value overshoot(const Value &x, const Value &fs = Value::Empty,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Percent undershoot below the lower state level.
///
/// Mirror of @ref overshoot for downward transitions.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → samples.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of percentages.
/// @see overshoot
Value undershoot(const Value &x, const Value &fs = Value::Empty,
                 std::pmr::memory_resource *mr = nullptr);

/// Settling time of each transition.
///
/// Time from the start of each transition until `x` stays within `tol`
/// (default 2%) of the destination state.
///
/// @param x    Real 1-D signal.
/// @param fs   Sample rate in Hz. `Value::Empty` → samples.
/// @param tol  Settling tolerance as a fraction. Default 0.02.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Column vector, one entry per transition.
///
/// @see risetime
Value settlingtime(const Value &                x,
                   const Value &                fs = Value::Empty,
                   double                       tol = 0.02,
                   std::pmr::memory_resource *  mr  = nullptr);

/// Duration each pulse stays above the mid-state.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → samples.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector, one entry per pulse.
///
/// @see pulseperiod, dutycycle
Value pulsewidth(const Value &                x,
                 const Value &                fs = Value::Empty,
                 std::pmr::memory_resource *  mr = nullptr);

/// @brief Period between consecutive same-direction mid-state crossings.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → samples.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of periods, one per consecutive pulse pair.
/// @see pulsewidth, dutycycle
Value pulseperiod(const Value &x, const Value &fs = Value::Empty,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Separation between consecutive pulses (time below mid-state).
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → samples.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of separations.
/// @see pulsewidth, pulseperiod
Value pulsesep(const Value &x, const Value &fs = Value::Empty,
               std::pmr::memory_resource *mr = nullptr);

/// Duty cycle: fraction of each pulse period spent above the mid-state.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate in Hz. `Value::Empty` → samples (units cancel
///            in the ratio).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of values in [0, 1], one entry per period.
///
/// @see pulsewidth, pulseperiod
Value dutycycle(const Value &                x,
                const Value &                fs = Value::Empty,
                std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
