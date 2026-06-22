// ops/root_solve.hpp
//
// Scalar / low-dim iterative solver KERNELS (raw double precision, FnHandle
// objective) — the numerical core behind fzero / fminbnd / fminsearch. Core-free
// (value layer only); the toolbox Value-API wrappers and the engine adapters live
// above. Solver kernels belong in ops per LAYERING_TARGET_ARCHITECTURE §2/§3/§9.

#pragma once

#include <cstddef>
#include <memory_resource>
#include <numkit/value/fn_handle.hpp>
#include <numkit/value/scratch.hpp>
#include <utility>

namespace numkit::ops {

/// Expand a bracket around x0 by stepping outward until a sign change; returns
/// {a, b} with f(a)*f(b) <= 0 (a==b when a root is hit exactly). Throws if none
/// is found within the iteration cap.
std::pair<double, double> findBracket(FnHandle fn, double x0, std::pmr::memory_resource *mr);

/// Brent's method root on [a, b] (requires f(a)*f(b) < 0, or one endpoint == 0).
double brent(FnHandle fn, double a, double b, std::pmr::memory_resource *mr);

/// Bounded scalar minimum on [a, b] via Brent's golden-section + parabolic
/// interpolation hybrid (the fminbnd kernel).
double brentMin(FnHandle fn, double a, double b, double tol, std::pmr::memory_resource *mr);

/// Unconstrained multi-dim minimum from x0[0..n) via Nelder-Mead (the fminsearch
/// kernel). Returns the best vertex (length n), allocated on `mr`.
ScratchVec<double> nelderMead(FnHandle fn, const double *x0, std::size_t n, double tol,
                              std::pmr::memory_resource *mr);

} // namespace numkit::ops
