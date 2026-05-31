// libs/ode/include/numkit/ode/options.hpp
//
// odeset / odeget — assembled options struct passed to the ode* solvers.
//
// Reference:
//   • Shampine & Reichelt, "The MATLAB ODE Suite", SIAM J. Sci. Comput.
//     18(1), 1997.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::ode {

/// @brief Build / extend an ODE options struct (`odeset(...)`).
///
/// MATLAB signatures supported:
///   * `opts = odeset()` — return defaults.
///   * `opts = odeset('Name', Value, ...)` — name-value pairs.
///   * `opts = odeset(old, 'Name', Value, ...)` — extend an existing struct.
///   * `opts = odeset(old, new)` — merge two structs (new wins).
///
/// Recognised names (case-insensitive, MATLAB convention):
///   RelTol, AbsTol, NormControl, MaxStep, InitialStep, NonNegative,
///   Refine, Stats, OutputFcn, OutputSel, Mass, MStateDependence,
///   MvPattern, MassSingular, InitialSlope, Vectorized, Events,
///   BDF, MaxOrder, Jacobian, JPattern.
///
/// Unknown names throw `m:odeset:badName`. Unset names default to `[]`.
Value odeset(const Value *args, std::size_t nargs,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Read a single field from an ODE options struct
/// (`val = odeget(opts, 'Name'[, default])`).
///
/// Returns the value of `Name` from `opts`, or `default` (or `[]`) if
/// unset. Case-insensitive name lookup.
Value odeget(const Value &opts, const Value &name,
             const Value &default_v, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::ode
