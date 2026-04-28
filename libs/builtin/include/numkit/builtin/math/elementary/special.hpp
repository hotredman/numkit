// libs/builtin/include/numkit/builtin/math/elementary/special.hpp
//
// Special functions: gamma, gammaln, erf, erfc, erfinv. Real-only;
// std::tgamma / std::lgamma / std::erf / std::erfc backed. erfinv is
// Winitzki + 3 Newton steps.

#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// gamma(x)   — Γ(x). Real-only; tgamma backed.
Value gammaFn(Allocator &alloc, const Value &x);

/// gammaln(x) — log|Γ(x)|. Real-only; lgamma backed.
Value gammaln(Allocator &alloc, const Value &x);

/// erf(x)     — error function 2/√π ∫₀ˣ e^{-t²} dt.
Value erf(Allocator &alloc, const Value &x);

/// erfc(x)    — 1 - erf(x), accurate for large x.
Value erfc(Allocator &alloc, const Value &x);

/// erfinv(y)  — inverse error function on (-1, 1).
///              y == ±1 → ±Inf, |y| > 1 (real input) → NaN.
///              Implementation: Winitzki's closed-form initial estimate
///              followed by 3 Newton steps for full double precision.
Value erfinv(Allocator &alloc, const Value &x);

} // namespace numkit::builtin
