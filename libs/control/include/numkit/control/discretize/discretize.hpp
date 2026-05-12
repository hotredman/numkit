// libs/control/include/numkit/control/discretize/discretize.hpp
//
// Sample-time conversion: c2d (continuous → discrete) and d2c
// (discrete → continuous). Returns the same kind of LTI struct as
// the input (tf in → tf out, ss in → ss out, zpk in → zpk out).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::control {

/// `c2d(sys, Ts [, method])` — continuous → discrete.
///   method = "zoh"     (default): zero-order hold via Van Loan expm.
///   method = "tustin"            : bilinear,
///                                  z = (1 + s·Ts/2) / (1 − s·Ts/2).
Value c2d(const Value &sys, double Ts, const std::string &method, std::pmr::memory_resource *mr = nullptr);

/// `d2c(sys [, method])` — discrete → continuous.
///   method = "tustin"            : bilinear (default).
///   method = "zoh"               : not implemented (would require
///                                  a matrix logarithm).
Value d2c(const Value &sys, const std::string &method, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
