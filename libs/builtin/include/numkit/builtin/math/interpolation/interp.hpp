// libs/builtin/include/numkit/builtin/math/interpolation/interp.hpp
//
// 1-D / 2-D / 3-D interpolation. polyfit / polyval moved to
// math/elementary/polynomials.hpp; trapz moved to
// math/integration/integration.hpp.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::builtin {

/// 1D interpolation at query points xq.
///
/// @param method  "linear" (default), "nearest", "spline", "pchip".
/// @throws Error on shape mismatch or unknown method.
Value interp1(std::pmr::memory_resource *mr,
               const Value &x,
               const Value &y,
               const Value &xq,
               const std::string &method = "linear");

/// 2D interpolation at query points (Xq, Yq).
///
///   interp2(V, Xq, Yq[, method])         — implicit X = 1:cols(V), Y = 1:rows(V).
///   interp2(X, Y, V, Xq, Yq[, method])   — explicit grid (X / Y must be vectors
///                                           giving column / row coordinates,
///                                           strictly monotonic ascending).
///
/// Supported methods: "linear" (default, bilinear), "nearest". Output
/// shape matches Xq (which must broadcast-shape-equal Yq). Out-of-grid
/// queries return NaN. V must be a real 2D matrix.
Value interp2(std::pmr::memory_resource *mr,
               const Value &V, const Value &Xq, const Value &Yq,
               const std::string &method = "linear");
Value interp2(std::pmr::memory_resource *mr,
               const Value &X, const Value &Y, const Value &V,
               const Value &Xq, const Value &Yq,
               const std::string &method = "linear");

/// interp3(V, Xq, Yq, Zq[, method]) — implicit 1:N grids.
/// interp3(X, Y, Z, V, Xq, Yq, Zq[, method]) — explicit grids.
///
/// Trilinear by default; method = "linear" or "nearest". X/Y/Z are
/// vectors giving column / row / page coordinates (strictly monotonic
/// ascending). Grid sizes must equal cols(V), rows(V), pages(V)
/// respectively. Out-of-grid query points return NaN.
Value interp3(std::pmr::memory_resource *mr, const Value &V,
               const Value &Xq, const Value &Yq, const Value &Zq,
               const std::string &method = "linear");
Value interp3(std::pmr::memory_resource *mr, const Value &X, const Value &Y, const Value &Z,
               const Value &V, const Value &Xq, const Value &Yq, const Value &Zq,
               const std::string &method = "linear");

/// Natural cubic-spline interpolation — equivalent to interp1(..., "spline").
Value spline(std::pmr::memory_resource *mr, const Value &x, const Value &y, const Value &xq);

/// Piecewise cubic Hermite — equivalent to interp1(..., "pchip").
Value pchip(std::pmr::memory_resource *mr, const Value &x, const Value &y, const Value &xq);

} // namespace numkit::builtin
