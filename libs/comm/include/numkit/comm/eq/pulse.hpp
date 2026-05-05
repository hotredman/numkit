// libs/comm/include/numkit/comm/eq/pulse.hpp
//
// Pulse-shaping filter design (raised-cosine and root-raised-cosine).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::comm {

/// `h = rcosdesign(beta, span, sps [, shape])` — design a raised-
/// cosine FIR filter.
///   beta  : roll-off factor, 0 ≤ β ≤ 1
///   span  : filter span in symbol periods (positive integer)
///   sps   : samples per symbol (positive integer)
///   shape : "normal" (default — raised cosine, RC) or
///           "sqrt" (root raised cosine, RRC)
/// Returns a row vector of length `span * sps + 1`. RRC coefficients
/// are normalised so ‖h‖² = 1 (unit-energy); RC coefficients are
/// normalised so ‖h‖² = 1 as well, matching MATLAB R2025b behaviour.
Value rcosdesign(std::pmr::memory_resource *mr,
                 double beta, int span, int sps,
                 const std::string &shape);

/// `h = gaussdesign(BT, span, sps)` — design a Gaussian FIR pulse-
/// shaping filter.
///   BT    : 3-dB bandwidth × symbol period (typical values 0.1..0.5)
///   span  : filter span in symbol periods (positive integer)
///   sps   : samples per symbol (positive integer)
/// Returns a row vector of length `span * sps + 1`, sum-normalised
/// to 1 (matches MATLAB R2025b's gaussdesign).
Value gaussdesign(std::pmr::memory_resource *mr,
                  double BT, int span, int sps);

/// `y = rectpulse(x, n)` — rectangular pulse shaping. Each input
/// sample is repeated `n` times along the leading non-singleton
/// dimension. For an L×1 column input → (L·n)×1 output; for 1×L row
/// → 1×(L·n); for matrices each column is repeated row-wise (matches
/// MATLAB R2025b's rectpulse).
Value rectpulse(std::pmr::memory_resource *mr, const Value &x, int n);

} // namespace numkit::comm
