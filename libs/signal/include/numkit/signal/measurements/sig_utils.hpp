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

} // namespace numkit::signal
