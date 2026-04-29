// libs/builtin/include/numkit/builtin/math/integration/integration.hpp
//
// Numerical integration / differentiation builtins:
//   gradient / gradient2 — finite-difference derivatives
//   cumtrapz             — cumulative trapezoidal integration
//   integral             — adaptive Gauss-Kronrod definite integral

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit { class Engine; }

namespace numkit::builtin {

using ::numkit::Engine;

/// gradient(F[, h]) — central differences in the interior, one-sided
/// at the endpoints. Default spacing h = 1.
///   1-D vector input → 1-D gradient.
///   2-D matrix input → ∂F/∂x (along dim-2, columns). MATLAB convention.
/// Output is DOUBLE, same shape as F.
Value gradient(std::pmr::memory_resource *mr, const Value &f, double h = 1.0);

/// [Fx, Fy] = gradient2(F[, hx, hy]) — 2-D gradients along dim-2 and
/// dim-1 respectively (MATLAB ordering: x-direction first).
std::tuple<Value, Value>
gradient2(std::pmr::memory_resource *mr, const Value &f, double hx = 1.0, double hy = 1.0);

/// cumtrapz(y) / cumtrapz(x, y) — cumulative trapezoidal integration.
/// One-arg form uses unit spacing; two-arg form uses the spacing from x.
/// 1-D vector input only for now (matrix support deferred).
Value cumtrapz(std::pmr::memory_resource *mr, const Value &y);
Value cumtrapz(std::pmr::memory_resource *mr, const Value &x, const Value &y);

/// integral(fn, a, b[, absTol]) — definite integral via adaptive
/// Gauss-Kronrod quadrature (15-point Kronrod with embedded 7-point
/// Gauss). Recurses on subintervals where the absolute difference
/// between G and K exceeds absTol. Default absTol = 1e-10.
/// Up to ~16 subdivision levels per branch.
Value integral(std::pmr::memory_resource *mr, const Value &fn, double a, double b,
                double absTol, Engine *engine);

/// trapz(y) — trapezoidal numerical integration over uniform spacing (dx = 1).
Value trapz(std::pmr::memory_resource *mr, const Value &y);

/// trapz(x, y) — trapezoidal numerical integration with explicit x values.
/// @throws Error if numel(x) != numel(y).
Value trapz(std::pmr::memory_resource *mr, const Value &x, const Value &y);

} // namespace numkit::builtin
