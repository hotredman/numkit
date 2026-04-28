// libs/builtin/include/numkit/m/builtin/MStdCalculus.hpp
//
// Numerical-calculus builtins: gradient, cumtrapz (more in tier-2/3).

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::builtin {

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

} // namespace numkit::m::builtin
