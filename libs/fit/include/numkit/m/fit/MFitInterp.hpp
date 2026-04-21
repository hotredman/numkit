// libs/fit/include/numkit/m/fit/MFitInterp.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <string>

namespace numkit::m::fit {

/// 1D interpolation at query points xq.
///
/// @param method  "linear" (default), "nearest", "spline", "pchip".
/// @throws MError on shape mismatch or unknown method.
MValue interp1(Allocator &alloc,
               const MValue &x,
               const MValue &y,
               const MValue &xq,
               const std::string &method = "linear");

/// Natural cubic-spline interpolation — equivalent to interp1(..., "spline").
MValue spline(Allocator &alloc, const MValue &x, const MValue &y, const MValue &xq);

/// Piecewise cubic Hermite — equivalent to interp1(..., "pchip").
MValue pchip(Allocator &alloc, const MValue &x, const MValue &y, const MValue &xq);

/// Least-squares polynomial fit of degree n. Returns coefficient row vector
/// in descending power order (p[0] * x^n + p[1] * x^(n-1) + ...).
///
/// @throws MError on singular normal-matrix (ill-conditioned) or not enough
///         data points (need at least n+1).
MValue polyfit(Allocator &alloc, const MValue &x, const MValue &y, int n);

/// Horner evaluation of polynomial p at x. Returns array same shape as x.
MValue polyval(Allocator &alloc, const MValue &p, const MValue &x);

/// Trapezoidal numerical integration over uniform spacing (dx = 1).
MValue trapz(Allocator &alloc, const MValue &y);

/// Trapezoidal numerical integration with explicit x values.
/// @throws MError if numel(x) != numel(y).
MValue trapz(Allocator &alloc, const MValue &x, const MValue &y);

} // namespace numkit::m::fit
