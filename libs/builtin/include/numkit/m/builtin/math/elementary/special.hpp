// libs/builtin/include/numkit/m/builtin/math/elementary/special.hpp
//
// Special functions: gamma, gammaln, erf, erfc, erfinv. Real-only;
// std::tgamma / std::lgamma / std::erf / std::erfc backed. erfinv is
// Winitzki + 3 Newton steps.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

/// gamma(x)   — Γ(x). Real-only; tgamma backed.
MValue gammaFn(Allocator &alloc, const MValue &x);

/// gammaln(x) — log|Γ(x)|. Real-only; lgamma backed.
MValue gammaln(Allocator &alloc, const MValue &x);

/// erf(x)     — error function 2/√π ∫₀ˣ e^{-t²} dt.
MValue erf(Allocator &alloc, const MValue &x);

/// erfc(x)    — 1 - erf(x), accurate for large x.
MValue erfc(Allocator &alloc, const MValue &x);

/// erfinv(y)  — inverse error function on (-1, 1).
///              y == ±1 → ±Inf, |y| > 1 (real input) → NaN.
///              Implementation: Winitzki's closed-form initial estimate
///              followed by 3 Newton steps for full double precision.
MValue erfinv(Allocator &alloc, const MValue &x);

} // namespace numkit::m::builtin
