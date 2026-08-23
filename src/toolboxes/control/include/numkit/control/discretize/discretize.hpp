/// @file discretize.hpp
/// @ingroup group_control
// toolboxes/control/include/numkit/control/discretize/discretize.hpp
//
// Sample-time conversion: c2d (continuous → discrete) and d2c
// (discrete → continuous). Returns the same kind of LTI struct as
// the input (tf in → tf out, ss in → ss out, zpk in → zpk out).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::control {

/// Continuous-to-discrete conversion (`c2d(sys, Ts, method)`).
///
/// Discretises a continuous-time LTI model at sample time `Ts`. The
/// output struct's `kind` matches the input (tf → tf, zpk → zpk, ss → ss);
/// the conversion is always done through an internal ss form, then
/// converted back if needed.
///
/// Supported methods:
///   - `"zoh"` (default): zero-order hold via Van Loan matrix exponential
///     of @f$ \exp\!\left(\begin{smallmatrix}A & B \\ 0 & 0\end{smallmatrix}\right) T_s @f$.
///   - `"tustin"`: bilinear (Tustin), @f$ z = (1 + sT_s/2)/(1 - sT_s/2) @f$.
///
/// @param sys     Continuous LTI struct (`Ts == 0`).
/// @param Ts      Sample time in seconds (positive).
/// @param method  `"zoh"` or `"tustin"` (case-sensitive).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Discrete LTI struct with `Ts` set.
/// @throws        Error on unknown method, non-continuous input, or
///                singularity in the Tustin matrix `I − A·Ts/2`.
///
/// @code
/// auto sysC = tf({1}, {1, 2, 1});      // continuous plant
/// auto sysD = c2d(sysC, 0.1, "zoh");   // discrete @ 0.1 s
/// @endcode
///
/// @see d2c
Value c2d(const Value &sys, double Ts, const std::string &method,
          std::pmr::memory_resource *mr = nullptr);

/// Discrete-to-continuous conversion (`d2c(sys, method)`).
///
/// Inverse of @ref c2d. The default method is `"tustin"`; ZOH inverse
/// is not implemented (would require a numerically robust matrix
/// logarithm).
///
/// @param sys     Discrete LTI struct (`Ts > 0`).
/// @param method  `"tustin"` (default). `"zoh"` is rejected.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Continuous LTI struct (`Ts == 0`).
/// @throws        Error on unknown method or non-discrete input.
///
/// @see c2d
Value d2c(const Value &sys, const std::string &method,
          std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
