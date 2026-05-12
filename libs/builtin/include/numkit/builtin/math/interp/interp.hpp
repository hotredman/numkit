// libs/builtin/include/numkit/builtin/math/interp/interp.hpp
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
Value interp1(const Value &x, const Value &y, const Value &xq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

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
Value interp2(const Value &V, const Value &Xq, const Value &Yq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);
Value interp2(const Value &X, const Value &Y, const Value &V, const Value &Xq, const Value &Yq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// interp3(V, Xq, Yq, Zq[, method]) — implicit 1:N grids.
/// interp3(X, Y, Z, V, Xq, Yq, Zq[, method]) — explicit grids.
///
/// Trilinear by default; method = "linear" or "nearest". X/Y/Z are
/// vectors giving column / row / page coordinates (strictly monotonic
/// ascending). Grid sizes must equal cols(V), rows(V), pages(V)
/// respectively. Out-of-grid query points return NaN.
Value interp3(const Value &V, const Value &Xq, const Value &Yq, const Value &Zq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);
Value interp3(const Value &X, const Value &Y, const Value &Z, const Value &V, const Value &Xq, const Value &Yq, const Value &Zq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// Natural cubic-spline interpolation — equivalent to interp1(..., "spline").
Value spline(const Value &x, const Value &y, const Value &xq, std::pmr::memory_resource *mr = nullptr);

/// Piecewise cubic Hermite — equivalent to interp1(..., "pchip").
Value pchip(const Value &x, const Value &y, const Value &xq, std::pmr::memory_resource *mr = nullptr);

//// mkpp(breaks, coefs) — build a MATLAB-style pp struct with fields
//// {form='pp', breaks, coefs, pieces, order, dim}. coefs is pieces×order.
Value mkpp(const Value &breaks, const Value &coefs, std::pmr::memory_resource *mr = nullptr);

/// ppval(pp, x) — evaluate the piecewise polynomial in pp at every
/// point of x via local Horner. Output shape mirrors x.
Value ppval(const Value &pp, const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
