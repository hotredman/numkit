// libs/signal/include/numkit/signal/measurements/sig_utils.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <utility>

namespace numkit::signal {

/// Find the smallest repetition period of a signal.
///
/// Returns the smallest integer `d` ≤ N such that
/// `|x[i] - x[i mod d]| ≤ tol` for every i. Useful for detecting if a
/// captured waveform is periodic.
///
/// @param x    Real 1-D signal of length N.
/// @param tol  Maximum allowed deviation between equivalent samples.
///             Default 0.0 (exact match).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Pair `(period, numRepetitions)` where
///             `numRepetitions = N / period` (as DOUBLE scalars).
///
/// @note Vector input only in v1. The matrix (column-wise) form
///       is deferred.
std::pair<Value, Value>
seqperiod(const Value &                x,
          double                       tol = 0.0,
          std::pmr::memory_resource *  mr  = nullptr);

/// Zero-crossing rate of a signal.
///
/// Counts sign changes of `(x - level)` over the input and returns the
/// rate per sample. Boundary half-credit `+0.5` is applied for the
/// default `ZeroPositive = false` setting.
///
/// @param x      Real 1-D signal.
/// @param level  Threshold around which crossings are counted. Default 0.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Pair `(rate, count)` where `rate = count / numel(x)`.
///
/// @note Name=Value args (`'Threshold'`, `'TransitionEdge'`,
///       `'WindowLength'`) and matrix input deferred.
std::pair<Value, Value>
zerocrossrate(const Value &                x,
              double                       level = 0.0,
              std::pmr::memory_resource *  mr    = nullptr);

/// @brief Result of `cusum` — `[iupper, ilower, uppersum, lowersum]`.
struct CusumResult {
    Value iupper;    ///< First index where the upper sum exceeds climit (empty = none).
    Value ilower;    ///< First index where the lower sum exceeds climit (empty = none).
    Value uppersum;  ///< Length-N upper cumulative sum.
    Value lowersum;  ///< Length-N lower cumulative sum.
};

/// @brief CUSUM change detector
/// (`[iu, il, us, ls] = cusum(x, climit, mshift, tmean, tdev)`).
///
/// Standardises `x` to `(x - tmean) / tdev`, accumulates the one-sided CUSUM
/// statistics with an `mshift/2` slack, and reports the first sample index at
/// which each one-sided sum first exceeds `climit`. The target mean / std
/// default to the mean / std of the first 25 samples.
///
/// @param x       Input signal.
/// @param climit  Control limit in standard-deviation units (default `5`).
/// @param mshift  Mean shift to detect, in standard-deviation units (default `1`).
/// @param tmean   Target mean (`Value::Empty` → `mean(x(1:25))`).
/// @param tdev    Target std-dev (`Value::Empty` → `std(x(1:25))`).
/// @param mr      Memory resource (nullptr → process default).
/// @return        @ref CusumResult `{ iupper, ilower, uppersum, lowersum }`.
CusumResult cusum(const Value &x, double climit = 5.0, double mshift = 1.0,
                  const Value &tmean = Value::Empty,
                  const Value &tdev = Value::Empty,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
