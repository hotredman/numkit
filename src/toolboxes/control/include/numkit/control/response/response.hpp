/// @file response.hpp
/// @ingroup group_control
// toolboxes/control/include/numkit/control/response/response.hpp
//
// Time-domain responses for continuous and discrete LTI systems.
// Returns column-vector outputs y(t) and the matching time grid t.
// The implementation discretises continuous models via zero-order
// hold using a Van-Loan style matrix exponential (Padé with scaling
// and squaring), then iterates the discrete recurrence.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <utility>

namespace numkit::control {

/// Unit-step response (`[y, t] = step(sys, tFinal_or_t)`).
///
/// Simulates the response of `sys` to a unit step input. The time
/// argument is overloaded:
///   - scalar `Tfinal` → simulate from 0 to `Tfinal` with auto-picked dt
///     (slowest stable pole sets Tfinal, fastest pole sets dt);
///   - vector → use as the explicit time grid (uniform stride
///     expected for continuous models — mean dt is taken);
///   - empty / missing → auto-pick `Tfinal ≈ 8 / |Re λ_min|`.
///
/// Continuous models are discretised via ZOH using a Van Loan
/// matrix-exponential expansion; discrete models iterate the
/// recurrence directly.
///
/// @param sys   LTI struct (tf / zpk / ss).
/// @param tArg  Time argument (scalar / vector / empty).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `(y, t)`; bind via `auto [y, t] = step_response(sys, tArg);`.
///
/// @code
/// auto [y, t] = step_response(plant, Value::Empty);  // auto grid
/// @endcode
///
/// @see impulse_response, lsim
std::pair<Value, Value>
step_response(const Value &sys, const Value &tArg,
              std::pmr::memory_resource *mr = nullptr, Value *xOut = nullptr);

/// Unit-impulse response (`[y, t] = impulse(sys, tFinal_or_t)`).
///
/// Continuous: simulates with `u ≡ 0` and initial condition
/// `x(0+) = B` (the differentiation-through-delta trick).
/// Discrete: applies `u[0] = 1, u[k>0] = 0`.
///
/// Time argument semantics match @ref step_response.
///
/// @param sys   LTI struct.
/// @param tArg  Time argument.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `(y, t)`.
///
/// @see step_response, lsim
std::pair<Value, Value>
impulse_response(const Value &sys, const Value &tArg,
                 std::pmr::memory_resource *mr = nullptr, Value *xOut = nullptr);

/// Linear simulation with arbitrary input (`y = lsim(sys, u, t, x0)`).
///
/// Simulates @f$ \dot x = Ax + Bu,\ y = Cx + Du @f$ (or its discrete
/// analogue) along the explicit time grid `t`. `u` must have
/// `length(t)` rows (one row per sample). `x0` is the initial state
/// (length `n`); pass `Value::Empty` to default to zero.
///
/// @param sys  LTI struct.
/// @param u    Input samples, one row per time step.
/// @param t    Time grid (at least 2 samples).
/// @param x0   Initial state (or empty for zero IC).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Column vector of `y(t)` of length `length(t)`.
/// @throws     Error if `length(u) != length(t)` or `length(t) < 2`.
///
/// @see step_response, impulse_response
Value lsim(const Value &sys, const Value &u, const Value &t,
           const Value &x0,
           std::pmr::memory_resource *mr = nullptr, Value *xOut = nullptr);

/// Initial-condition response (`[y, t] = initial(sys, x0, tArg)`).
///
/// Time response of `sys` to the initial state `x0` with zero input —
/// the same integrator as @ref step_response / @ref impulse_response run
/// with `u ≡ 0` and `x(0) = x0`, output `y = C·x`. The time argument
/// `tArg` follows the same semantics as @ref step_response (`Value::Empty`
/// → auto grid from the system poles; scalar → final time; vector →
/// explicit grid). The optional state trajectory is returned via `xOut`.
///
/// @param sys   LTI struct (tf / zpk / ss).
/// @param x0    Initial state (length = number of states).
/// @param tArg  Time argument (Empty / scalar tFinal / explicit grid).
/// @param mr    Memory resource (nullptr → process default).
/// @param xOut  Optional state-trajectory output (one row per sample).
/// @return      `(y, t)`; bind via `auto [y, t] = initial_response(sys, x0, tArg);`.
/// @see step_response, impulse_response, lsim
std::pair<Value, Value>
initial_response(const Value &sys, const Value &x0, const Value &tArg,
                 std::pmr::memory_resource *mr = nullptr, Value *xOut = nullptr);

} // namespace numkit::control
