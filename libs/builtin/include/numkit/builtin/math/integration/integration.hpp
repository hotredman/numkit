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
Value gradient(const Value &f, double h = 1.0, std::pmr::memory_resource *mr = nullptr);

/// [Fx, Fy] = gradient2(F[, hx, hy]) — 2-D gradients along dim-2 and
/// dim-1 respectively (MATLAB ordering: x-direction first).
std::tuple<Value, Value>
gradient2(const Value &f, double hx = 1.0, double hy = 1.0, std::pmr::memory_resource *mr = nullptr);

/// cumtrapz(y) / cumtrapz(x, y) — cumulative trapezoidal integration.
/// One-arg form uses unit spacing; two-arg form uses the spacing from x.
/// Vector inputs preserve their shape; matrix inputs integrate down each
/// column (MATLAB's default along the first non-singleton dim). For
/// matrix y, x may be a column-length vector (broadcast per column) or
/// a matrix of the same shape as y (per-column spacing).
Value cumtrapz(const Value &y, std::pmr::memory_resource *mr = nullptr);
Value cumtrapz(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// integral(fn, a, b[, absTol]) — definite integral via adaptive
/// Gauss-Kronrod quadrature (15-point Kronrod with embedded 7-point
/// Gauss). Recurses on subintervals where the absolute difference
/// between G and K exceeds absTol. Default absTol = 1e-10.
/// Up to ~16 subdivision levels per branch.
Value integral(const Value &fn, double a, double b, double absTol, Engine *engine, std::pmr::memory_resource *mr = nullptr);

/// trapz(y) — trapezoidal numerical integration over uniform spacing (dx = 1).
Value trapz(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// trapz(x, y) — trapezoidal numerical integration with explicit x values.
/// @throws Error if numel(x) != numel(y).
Value trapz(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
