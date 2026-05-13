// libs/optim/include/numkit/optim/local/fzero.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit { class Engine; }

namespace numkit::optim {

using ::numkit::Engine;

/// @file
/// @brief Scalar root-finding and unconstrained minimisation.
///
/// All functions require an `Engine *` — they invoke the user-supplied
/// function-handle callback through the engine. In the registered
/// builtin path this comes from `CallContext::engine`.

/// @brief Scalar root-finder (`x = fzero(fn, x0_or_interval)`).
///
/// - `fzero(fn, x0)`     — root near `x0`. Expands an initial bracket
///   around `x0` until a sign change is found, then runs Brent's method.
/// - `fzero(fn, [a b])`  — root inside the interval `[a, b]`. Throws if
///   `sign(fn(a)) == sign(fn(b))` (no obvious root).
///
/// @param fn              Function handle taking a scalar, returning scalar.
/// @param x0OrInterval    Either a scalar starting point or a 2-vector `[a, b]`.
/// @param engine          Engine context (used to invoke the handle).
/// @param mr              Memory resource (nullptr → process default).
/// @return                Scalar root.
/// @throws Error          No sign change in user-supplied interval.
/// @see fminbnd
Value fzero(const Value &fn, const Value &x0OrInterval,
            Engine *engine,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Bounded scalar minimisation (`x = fminbnd(fn, lo, hi, tol)`).
///
/// Brent's golden-section + parabolic-interpolation hybrid on `[lo, hi]`.
///
/// @param fn       Function handle (scalar → scalar).
/// @param lo       Lower bound.
/// @param hi       Upper bound (`hi > lo`).
/// @param tol      Convergence tolerance on the minimiser location.
/// @param engine   Engine context.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Scalar minimiser `x*` ∈ `[lo, hi]`.
/// @see fzero, fminsearch
Value fminbnd(const Value &fn, double lo, double hi, double tol,
              Engine *engine,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Multi-dimensional unconstrained minimisation
/// (`x = fminsearch(fn, x0, tol)`).
///
/// Nelder-Mead simplex starting at `x0` (column vector).
///
/// @param fn       Function handle (vector → scalar).
/// @param x0       Starting point (column vector of any length ≥ 1).
/// @param tol      Convergence tolerance.
/// @param engine   Engine context.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Minimiser vector, same shape as `x0`.
/// @see fminbnd
Value fminsearch(const Value &fn, const Value &x0, double tol,
                 Engine *engine,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::optim
