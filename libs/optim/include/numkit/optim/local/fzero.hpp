// libs/optim/include/numkit/optim/local/fzero.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/fn_handle.hpp>
#include <numkit/core/value.hpp>

namespace numkit::optim {

/// @file
/// @brief Scalar root-finding and unconstrained minimisation.
///
/// All functions take a @ref numkit::FnHandle callback for the user
/// function — no `Engine` dependency in the library API. The engine
/// adapter wraps a MATLAB function-handle Value in a lambda and
/// passes it as @ref FnHandle.

/// @brief Scalar root-finder (`x = fzero(fn, x0_or_interval)`).
///
/// - `fzero(fn, x0)`     — root near `x0`. Expands an initial bracket
///   around `x0` until a sign change is found, then runs Brent's
///   method.
/// - `fzero(fn, [a b])`  — root inside the interval `[a, b]`. Throws
///   if `sign(fn(a)) == sign(fn(b))` (no obvious root).
///
/// The callback receives a 1-element `args` (the scalar evaluation
/// point) and writes its scalar result into `outs[0]`.
///
/// @param fn              MATLAB-style callback (scalar in, scalar out).
/// @param x0OrInterval    Either a scalar starting point or a 2-vector
///                        `[a, b]`.
/// @param mr              Memory resource (nullptr → process default).
/// @return                Scalar root.
/// @throws Error          No sign change in user-supplied interval.
/// @see fminbnd
Value fzero(FnHandle fn, const Value &x0OrInterval,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Bounded scalar minimisation
/// (`x = fminbnd(fn, lo, hi, tol)`).
///
/// Brent's golden-section + parabolic-interpolation hybrid on
/// `[lo, hi]`.
///
/// @param fn   MATLAB-style callback (scalar in, scalar out).
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
/// Nelder-Mead simplex starting at `x0` (column vector). The
/// callback receives a 1-element `args` whose [0] entry is a column
/// vector Value of length `numel(x0)`, and writes a scalar value
/// into `outs[0]`.
///
/// @param fn   MATLAB-style callback (vector in, scalar out).
/// @param x0   Starting point (column vector of any length ≥ 1).
/// @param tol  Convergence tolerance.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Minimiser vector, same shape as `x0`.
/// @see fminbnd
Value fminsearch(FnHandle fn, const Value &x0, double tol,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::optim
