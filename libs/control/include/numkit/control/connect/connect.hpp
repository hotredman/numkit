// libs/control/include/numkit/control/connect/connect.hpp
//
// LTI block-diagram interconnections. All three return a `tf`
// struct regardless of input forms — converting tf/zpk/ss inputs
// to coefficient pairs first via the existing libs/* primitives.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// `series(sys1, sys2)` — cascade: u → sys1 → sys2.
/// Equivalent to sys2 * sys1 in transfer-function math.
Value series(const Value &sys1, const Value &sys2, std::pmr::memory_resource *mr = nullptr);

/// `parallel(sys1, sys2)` — both blocks see the same input; outputs
/// are summed. tf form: H = (n1·d2 + n2·d1) / (d1·d2).
Value parallel(const Value &sys1, const Value &sys2, std::pmr::memory_resource *mr = nullptr);

/// `feedback(G, H, sign)` — closed-loop transfer function.
/// `sign = -1` (default, MATLAB convention): T = G / (1 + G·H).
/// `sign = +1` (positive feedback)         : T = G / (1 − G·H).
Value feedback(const Value &G, const Value &H, int sign, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
