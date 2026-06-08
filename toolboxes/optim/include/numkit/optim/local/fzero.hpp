// toolboxes/optim/include/numkit/optim/local/fzero.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/fn_handle.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

namespace numkit::optim {

/// @file
/// @brief Scalar root-finding and unconstrained minimisation.
///
/// All functions take a @ref numkit::FnHandle callback for the user
/// function — no `Engine` dependency in the library API. The engine
/// adapter wraps a function-handle Value in a lambda and
/// passes it as @ref FnHandle.

/// @brief Scalar root-finder, initial-guess form (`x = fzero(fn, x0)`).
///
/// Expands an outward bracket around `x0` until a sign change is
/// detected, then runs Brent's method.
///
/// The callback receives a 1-element `args` (the scalar evaluation
/// point) and writes its scalar result into `outs[0]`.
///
/// @param fn   Callback (scalar in, scalar out).
/// @param x0   Initial guess.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Scalar root.
/// @throws Error  No sign change found near `x0`.
/// @see fminbnd
Value fzero(FnHandle fn, double x0,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Scalar root-finder, bracket form (`x = fzero(fn, a, b)`).
///
/// Runs Brent's method on `[a, b]`. Throws if `sign(fn(a)) ==
/// sign(fn(b))` (no sign change inside the interval).
///
/// @param fn   Callback (scalar in, scalar out).
/// @param a    Lower bracket bound (must be finite, `a < b`).
/// @param b    Upper bracket bound (must be finite, `a < b`).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Scalar root.
/// @throws Error  No sign change in `[a, b]`, or invalid bounds.
/// @see fminbnd
Value fzero(FnHandle fn, double a, double b,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Bounded scalar minimisation
/// (`x = fminbnd(fn, lo, hi, tol)`).
///
/// Brent's golden-section + parabolic-interpolation hybrid on
/// `[lo, hi]`.
///
/// @param fn   Callback (scalar in, scalar out).
/// @param lo   Lower bound.
/// @param hi   Upper bound (`hi > lo`).
/// @param tol  Convergence tolerance on the minimiser location.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Scalar minimiser `x*` ∈ `[lo, hi]`.
/// @see fzero, fminsearch
Value fminbnd(FnHandle fn, double lo, double hi, double tol,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Multi-dimensional unconstrained minimisation
/// (`x = fminsearch(fn, x0, tol)`).
///
/// Nelder-Mead simplex starting at `x0`. The callback receives a
/// 1-element `args` whose `[0]` entry is a `1 × n` DOUBLE row Value
/// of length `n = x0.size()`, and writes a scalar into `outs[0]`.
///
/// @param fn   Callback (vector in, scalar out).
/// @param x0   Starting point (any length ≥ 1).
/// @param tol  Convergence tolerance.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Minimiser as a `n × 1` DOUBLE column Value (length
///             `n = x0.size()`).
/// @see fminbnd
Value fminsearch(FnHandle fn, Span<const double> x0, double tol,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::optim
