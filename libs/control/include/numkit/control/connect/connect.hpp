// libs/control/include/numkit/control/connect/connect.hpp
//
// LTI block-diagram interconnections. All three return a `tf`
// struct regardless of input forms — converting tf/zpk/ss inputs
// to coefficient pairs first via the existing libs/* primitives.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::control {

/// Cascade two LTI blocks (`series(sys1, sys2)`).
///
/// Implements the topology u → sys1 → sys2 → y, equivalent to
/// `sys2 * sys1` in transfer-function math
/// (@f$ H(s) = H_2(s)\,H_1(s) @f$).
///
/// Either input may be in any of the three LTI forms (tf / zpk / ss);
/// they are reduced to (num, den) coefficient pairs internally and
/// the polynomial product is taken. The result is always a `tf`
/// struct, even if both inputs were zpk or ss.
///
/// @param sys1  First block (input side).
/// @param sys2  Second block (output side).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `tf` struct for the cascaded system.
///
/// @code
/// auto C = tf({1, 1}, {1});            // PI controller numerator only
/// auto P = tf({1}, {1, 2, 1});         // 2nd-order plant
/// auto L = series(C, P);               // loop gain L = P·C
/// @endcode
///
/// @see parallel, feedback
Value series(const Value &sys1, const Value &sys2,
             std::pmr::memory_resource *mr = nullptr);

/// Parallel connection of two LTI blocks (`parallel(sys1, sys2)`).
///
/// Both blocks see the same input; their outputs are summed:
/// @f$ H(s) = H_1(s) + H_2(s) @f$. In coefficient form,
/// `H = (n1·d2 + n2·d1) / (d1·d2)` with leading-coefficient
/// normalisation on the denominator.
///
/// @param sys1  First block.
/// @param sys2  Second block.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `tf` struct for the parallel combination.
///
/// @see series, feedback
Value parallel(const Value &sys1, const Value &sys2,
               std::pmr::memory_resource *mr = nullptr);

/// Closed-loop transfer function under feedback (`feedback(G, H, sign)`).
///
/// Computes:
///   - sign = −1 (default, negative feedback): @f$ T = G / (1 + G\,H) @f$
///   - sign = +1 (positive feedback):          @f$ T = G / (1 - G\,H) @f$
///
/// `H` defaults to unity feedback at the call site (pass `tf({1},{1})`).
///
/// @param G     Forward path block.
/// @param H     Feedback path block.
/// @param sign  −1 (negative feedback) or +1 (positive feedback).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `tf` struct for the closed-loop system.
///
/// @code
/// auto P = tf({1},    {1, 2, 1});       // plant
/// auto C = tf({10, 1}, {1, 0});         // PI controller
/// auto T = feedback(series(C, P), tf({1},{1}), -1);  // unity-feedback CL
/// @endcode
///
/// @see series, parallel
Value feedback(const Value &G, const Value &H, int sign,
               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
