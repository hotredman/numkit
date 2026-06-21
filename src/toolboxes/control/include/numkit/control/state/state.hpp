// toolboxes/control/include/numkit/control/state/state.hpp
//
// State-space structural primitives: controllability and
// observability matrices, plus rank-based "is" predicates.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::control {

/// Controllability matrix from raw (A, B) (`Co = ctrb(A, B)`).
///
/// Builds @f$ \mathcal{C} = [B,\ AB,\ A^2 B,\ \ldots,\ A^{n-1} B] @f$
/// — shape `n × (n·m)` where `n = rows(A)` and `m = cols(B)`.
///
/// The system is controllable iff this matrix has full row rank
/// (`rank(Co) == n`). Numkit doesn't run the rank check here — that's
/// left to whatever rank tool the caller prefers.
///
/// @param A   n×n state matrix.
/// @param B   n×m input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    n × (n·m) controllability matrix.
///
/// @see ctrb_sys, obsv_AC
Value ctrb_AB(const Value &A, const Value &B,
              std::pmr::memory_resource *mr = nullptr);

/// Controllability matrix from a system struct (`Co = ctrb(sys)`).
///
/// Convenience wrapper: pulls (A, B) from the LTI struct (converting
/// tf / zpk inputs through @ref tf2ss internally) and delegates to
/// @ref ctrb_AB.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Controllability matrix.
///
/// @see ctrb_AB
Value ctrb_sys(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Observability matrix from raw (A, C) (`O = obsv(A, C)`).
///
/// Builds @f$ \mathcal{O} = [C;\ CA;\ CA^2;\ \ldots;\ CA^{n-1}] @f$
/// — shape `(n·p) × n` where `n = rows(A)` and `p = rows(C)`.
///
/// The system is observable iff this matrix has full column rank.
///
/// @param A   n×n state matrix.
/// @param C   p×n output matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    (n·p) × n observability matrix.
///
/// @see obsv_sys, ctrb_AB
Value obsv_AC(const Value &A, const Value &C,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Observability matrix from a system struct
/// (`O = obsv(sys)`).
///
/// Convenience wrapper analogous to @ref ctrb_sys.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Observability matrix.
/// @see obsv_AC
Value obsv_sys(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Controllability / observability gramian (`W = gram(sys, type)`).
///
/// Solves the appropriate Lyapunov equation for the LTI system `sys`:
/// - `type = "c"`: controllability gramian, @f$ A W_c + W_c A^{T} + B B^{T}
///   = 0 @f$ (discrete: Stein equation via @ref dlyap).
/// - `type = "o"`: observability gramian, @f$ A^{T} W_o + W_o A + C^{T} C
///   = 0 @f$.
///
/// Continuous vs discrete is taken from the system's sample time (`Ts`).
///
/// @param sys   LTI struct (ss / tf / zpk — converted to state space).
/// @param type  `"c"` (controllability) or `"o"` (observability).
/// @param mr    Memory resource (nullptr → process default).
/// @return      n×n gramian (symmetric positive-semidefinite for a stable sys).
/// @throws      Error on a non-LTI input or an unknown `type`.
/// @see lyap, dlyap, ctrb_sys, obsv_sys
Value gram(const Value &sys, const std::string &type,
           std::pmr::memory_resource *mr = nullptr);

/// Output and state covariance of an LTI system driven by white noise.
struct CovarResult {
    Value P;   ///< Steady-state output covariance.
    Value Q;   ///< Steady-state state covariance.
};

/// Steady-state covariance under white-noise excitation (`[P, Q] = covar(sys, W)`).
///
/// For an LTI system driven by white noise of intensity `W`, solves the
/// gramian Lyapunov equation for the state covariance `Q` and projects it
/// to the output covariance `P`:
/// - **continuous**: `A Q + Q Aᵀ + B W Bᵀ = 0` (via @ref lyap), `P = C Q Cᵀ`
///   (infinite if `D ≠ 0` — white noise through a direct feedthrough).
/// - **discrete**: `A Q Aᵀ − Q + B W Bᵀ = 0` (via @ref dlyap),
///   `P = C Q Cᵀ + D W Dᵀ`.
///
/// @param sys  LTI struct (ss / tf / zpk — converted to state space).
/// @param W    Noise intensity (scalar → `W·I`, or an m×m matrix).
/// @param mr   Memory resource (nullptr → process default).
/// @return     `{P, Q}` (see @ref CovarResult).
/// @throws     Error on a non-LTI input or a shape mismatch.
/// @see gram, lyap, dlyap
CovarResult covar(const Value &sys, const Value &W,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
