// libs/builtin/include/numkit/m/builtin/MStdCalculus.hpp
//
// Numerical-calculus builtins: gradient, cumtrapz (more in tier-2/3).

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m { class Engine; }

namespace numkit::m::builtin {

using ::numkit::m::Engine;

/// gradient(F[, h]) — central differences in the interior, one-sided
/// at the endpoints. Default spacing h = 1.
///   1-D vector input → 1-D gradient.
///   2-D matrix input → ∂F/∂x (along dim-2, columns). MATLAB convention.
/// Output is DOUBLE, same shape as F.
MValue gradient(Allocator &alloc, const MValue &f, double h = 1.0);

/// [Fx, Fy] = gradient2(F[, hx, hy]) — 2-D gradients along dim-2 and
/// dim-1 respectively (MATLAB ordering: x-direction first).
std::tuple<MValue, MValue>
gradient2(Allocator &alloc, const MValue &f, double hx = 1.0, double hy = 1.0);

/// cumtrapz(y) / cumtrapz(x, y) — cumulative trapezoidal integration.
/// One-arg form uses unit spacing; two-arg form uses the spacing from x.
/// 1-D vector input only for now (matrix support deferred).
MValue cumtrapz(Allocator &alloc, const MValue &y);
MValue cumtrapz(Allocator &alloc, const MValue &x, const MValue &y);

/// fzero(fn, x0)   — scalar root near x0. Expands an initial bracket
///                    around x0 until sign change is found, then runs
///                    Brent's method.
/// fzero(fn, [a, b]) — root inside the interval [a, b]. Throws if
///                    sign(fn(a)) == sign(fn(b)) (no obvious root).
/// `fn` must be a function handle. Engine pointer is required to invoke
/// the callback — it's expected to come from the CallContext.
MValue fzero(Allocator &alloc, const MValue &fn, const MValue &x0OrInterval,
             Engine *engine);

/// integral(fn, a, b[, absTol]) — definite integral via adaptive
/// Gauss-Kronrod quadrature (15-point Kronrod with embedded 7-point
/// Gauss). Recurses on subintervals where the absolute difference
/// between G and K exceeds absTol. Default absTol = 1e-10.
/// Up to ~16 subdivision levels per branch.
MValue integral(Allocator &alloc, const MValue &fn, double a, double b,
                double absTol, Engine *engine);

} // namespace numkit::m::builtin
