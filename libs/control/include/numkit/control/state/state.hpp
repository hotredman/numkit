// libs/control/include/numkit/control/state/state.hpp
//
// State-space structural primitives: controllability and
// observability matrices, plus rank-based "is" predicates.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

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

} // namespace numkit::control
