// libs/signal/include/numkit/signal/measurements/sig_utils.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <utility>

namespace numkit::signal {

/// seqperiod(x[, tol]) — find smallest period `d` ≤ N such that
/// x(i) ≈ x(((i-1) mod d) + 1) for all i within `tol`. Returns
/// (period, numRepetitions) where numRepetitions = N/period.
///
/// Vector input only (v1). MATLAB matrix form operates column-wise —
/// deferred.
std::pair<Value, Value>
seqperiod(const Value &                x,
          double                       tol = 0.0,
          std::pmr::memory_resource *  mr  = nullptr);

/// zerocrossrate(x[, level]) — count sign changes of (x - level) and
/// return (rate, count) where rate = count / numel(x). Boundary
/// half-credit (+0.5) applied per MATLAB R2025b default
/// `ZeroPositive=false`.
///
/// Vector input only (v1). Name=Value args ('Threshold',
/// 'TransitionEdge', 'WindowLength') deferred.
std::pair<Value, Value>
zerocrossrate(const Value &                x,
              double                       level = 0.0,
              std::pmr::memory_resource *  mr    = nullptr);

} // namespace numkit::signal
