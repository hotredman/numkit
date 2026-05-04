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

} // namespace numkit::comm
