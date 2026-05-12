// libs/control/include/numkit/control/response/response.hpp
//
// Time-domain responses for continuous and discrete LTI systems.
// Returns column-vector outputs y(t) and the matching time grid t.
// The implementation discretises continuous models via zero-order
// hold using a Van-Loan style matrix exponential (Padé with scaling
// and squaring), then iterates the discrete recurrence.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <utility>

namespace numkit::control {

/// `[y, t] = step(sys [, tFinal_or_t])` — unit-step response.
///
/// `tFinal_or_t`: scalar = simulate up to that time (auto-picks dt);
///                vector = use as the time grid (uniform stride
///                expected for continuous models — we use mean dt).
///                empty/missing = pick Tfinal ≈ 8/|Re λ_min|.
/// @return `(y, t)`; bind as `auto [y, t] = step_response(sys, tArg);`.
std::pair<Value, Value>
step_response(const Value &sys, const Value &tArg,
              std::pmr::memory_resource *mr = nullptr);

/// `[y, t] = impulse(sys [, tFinal_or_t])` — impulse response.
std::pair<Value, Value>
impulse_response(const Value &sys, const Value &tArg,
                 std::pmr::memory_resource *mr = nullptr);

/// `y = lsim(sys, u, t [, x0])` — general linear simulation.
/// `u` must have length(t) rows (one row per sample).
Value lsim(const Value &sys, const Value &u, const Value &t,
           const Value &x0,
           std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
