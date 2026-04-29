// libs/builtin/include/numkit/builtin/math/elementary/special.hpp
//
// Special functions: gamma, gammaln, erf, erfc, erfinv. Real-only;
// std::tgamma / std::lgamma / std::erf / std::erfc backed. erfinv is
// Winitzki + 3 Newton steps.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// gamma(x)   — Γ(x). Real-only; tgamma backed.
Value gammaFn(std::pmr::memory_resource *mr, const Value &x);

/// gammaln(x) — log|Γ(x)|. Real-only; lgamma backed.
Value gammaln(std::pmr::memory_resource *mr, const Value &x);

/// erf(x)     — error function 2/√π ∫₀ˣ e^{-t²} dt.
Value erf(std::pmr::memory_resource *mr, const Value &x);

/// erfc(x)    — 1 - erf(x), accurate for large x.
Value erfc(std::pmr::memory_resource *mr, const Value &x);

/// erfinv(y)  — inverse error function on (-1, 1).
///              y == ±1 → ±Inf, |y| > 1 (real input) → NaN.
///              Implementation: Winitzki's closed-form initial estimate
///              followed by 3 Newton steps for full double precision.
Value erfinv(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::builtin
