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

namespace numkit::control {

/// `[y, t] = step(sys [, tFinal_or_t])` — unit-step response.
/// `tFinal_or_t`: scalar = simulate up to that time (auto-picks dt);
///                vector = use as the time grid (uniform stride
///                expected for continuous models — we use mean dt).
///                empty/missing = pick Tfinal ≈ 8/|Re λ_min|.
void step_response(std::pmr::memory_resource *mr,
                   const Value &sys, const Value &tArg,
                   Value *yOut, Value *tOut);

/// `[y, t] = impulse(sys [, tFinal_or_t])` — impulse response.
void impulse_response(std::pmr::memory_resource *mr,
                      const Value &sys, const Value &tArg,
                      Value *yOut, Value *tOut);

/// `y = lsim(sys, u, t [, x0])` — general linear simulation.
/// `u` must have length(t) rows (one row per sample).
Value lsim(std::pmr::memory_resource *mr,
           const Value &sys, const Value &u, const Value &t,
           const Value &x0);

} // namespace numkit::control
