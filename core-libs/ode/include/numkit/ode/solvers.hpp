// toolboxes/ode/include/numkit/ode/solvers.hpp
//
// Initial-value problem (IVP) solvers for `y' = f(t, y)`.
//
// References:
//   • Hairer, Nørsett, Wanner, "Solving Ordinary Differential Equations I"
//     (Springer, 2nd ed. 1993) — DOPRI5 coefficients and step-size strategy.
//   • Dormand & Prince, "A family of embedded Runge-Kutta formulae",
//     J. Comput. Appl. Math. 6(1), 1980.
//   • Shampine & Reichelt, "The MATLAB ODE Suite", SIAM J. Sci. Comput.
//     18(1), 1997.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { class Engine; }

namespace numkit::ode {

/// @brief Explicit Dormand-Prince 5(4) RK solver
/// (`[t, y] = ode45(f, tspan, y0[, opts])`).
///
/// Solves the IVP `dy/dt = f(t, y)`, `y(t0) = y0` from `t0 = tspan(1)`
/// to `tf = tspan(end)`. The step size is chosen adaptively to keep
/// the local truncation error below
/// `max(RelTol · max(|y_old|, |y_new|), AbsTol)` per component.
///
/// Time output:
///   * `tspan = [t0, tf]`     → return every accepted step;
///   * `tspan = [t0, ..., tf]` (≥ 3 unique points)
///                            → return values only at `tspan`,
///                              computed by shortening the integration
///                              step to land exactly on each target.
///
/// The RHS is passed in as a MATLAB function-handle `Value` so the
/// solver can invoke it directly through `Engine::callFunctionHandle`.
/// The FnHandle wrapper path was abandoned because `function_ref`'s
/// type-erasure dropped func-handle semantics when the Engine
/// round-tripped back into the lambda.
///
/// @param eng    Engine used to dispatch the RHS function-handle.
/// @param fnh    Function-handle Value `@(t,y) ...` returning `dy/dt`.
/// @param tspan  Time vector (length ≥ 2, strictly monotonic).
/// @param y0     Initial state (column or row vector; flattened).
/// @param opts   Options struct from `odeset` (or `Value::Empty`).
/// @param mr     Memory resource.
/// @return       `{t, y}` — `t` is an `m × 1` column of accepted/sampled
///               times; `y` is an `m × d` matrix with each row a state.
std::tuple<Value, Value>
ode45(Engine &eng, const Value &fnh, const Value &tspan, const Value &y0,
      const Value &opts, std::pmr::memory_resource *mr = nullptr);

/// @brief Explicit Bogacki-Shampine 3(2) RK solver
/// (`[t, y] = ode23(f, tspan, y0[, opts])`).
///
/// Solves the IVP `dy/dt = f(t, y)`, `y(t0) = y0` from `t0 = tspan(1)`
/// to `tf = tspan(end)`. Uses the 4-stage embedded pair of Bogacki &
/// Shampine (1989) with the FSAL property, controlling local error
/// with the 2nd-order embedded estimate. Step-size control follows
/// the standard adaptive formula (Hairer-Nørsett-Wanner I, §II.4,
/// Eq. 4.13) with safety = 0.9 and 3rd-order exponent.
///
/// MATLAB ode23 default `Refine` = 1 (no interpolation between accepted
/// steps), unlike ode45 which defaults to 4. Override via `odeset`.
/// Dense output (used for `Refine > 1` or explicit `tspan` with ≥ 3
/// points) is the cubic Hermite interpolant between `(y_n, k1)` and
/// `(y_{n+1}, k4)`, which is 3rd-order accurate — matching MATLAB
/// (Shampine-Reichelt 1997).
///
/// @param eng    Engine used to dispatch the RHS function-handle.
/// @param fnh    Function-handle Value `@(t,y) ...` returning `dy/dt`.
/// @param tspan  Time vector (length ≥ 2, strictly monotonic).
/// @param y0     Initial state (column or row vector; flattened).
/// @param opts   Options struct from `odeset` (or `Value::Empty`).
/// @param mr     Memory resource.
/// @return       `{t, y}` — `t` is an `m × 1` column of accepted/sampled
///               times; `y` is an `m × d` matrix with each row a state.
std::tuple<Value, Value>
ode23(Engine &eng, const Value &fnh, const Value &tspan, const Value &y0,
      const Value &opts, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::ode
