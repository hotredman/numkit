// toolboxes/builtin/include/numkit/builtin/math/interp/interp.hpp
//
// 1-D / 2-D / 3-D interpolation. (polyfit / polyval moved to
// math/poly/polynomials.hpp; trapz moved to math/integration/integration.hpp.)

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit::math {

/// @brief 1-D interpolation (`yq = interp1(x, y, xq, method)`).
///
/// @param x       Sample sites (strictly monotonic).
/// @param y       Sample values (matching shape with `x`).
/// @param xq      Query points (any shape).
/// @param method  `"linear"` (default), `"nearest"`, `"spline"`, `"pchip"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Interpolated values, same shape as `xq`.
/// @throws Error  Shape mismatch or unknown method.
/// @see interp2, interp3, spline, pchip
Value interp1(const Value &x, const Value &y, const Value &xq,
              const std::string &method = "linear",
              std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D interpolation, implicit grid (`Vq = interp2(V, Xq, Yq, method)`).
///
/// Uses implicit `X = 1:cols(V)`, `Y = 1:rows(V)`. Supported methods:
/// `"linear"` (default, bilinear), `"nearest"`. Output shape matches
/// `Xq` (which must broadcast-shape-equal `Yq`). Out-of-grid queries
/// return `NaN`. `V` must be a real 2-D matrix.
///
/// @param V       Value matrix.
/// @param Xq      Column-coordinate query points.
/// @param Yq      Row-coordinate query points.
/// @param method  `"linear"` or `"nearest"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Interpolated values.
/// @see interp2(X, Y, V, Xq, Yq, method, mr), interp1
Value interp2(const Value &V, const Value &Xq, const Value &Yq,
              const std::string &method = "linear",
              std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D interpolation, explicit grid
/// (`Vq = interp2(X, Y, V, Xq, Yq, method)`).
///
/// `X` / `Y` are vectors giving column / row coordinates (strictly
/// monotonic ascending).
///
/// @param X       Column coordinates.
/// @param Y       Row coordinates.
/// @param V       Value matrix.
/// @param Xq      Column-coordinate query points.
/// @param Yq      Row-coordinate query points.
/// @param method  `"linear"` or `"nearest"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Interpolated values, shape matches `Xq` / `Yq`.
Value interp2(const Value &X, const Value &Y, const Value &V,
              const Value &Xq, const Value &Yq,
              const std::string &method = "linear",
              std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D interpolation, implicit grid
/// (`Vq = interp3(V, Xq, Yq, Zq, method)`).
///
/// Trilinear by default; method = `"linear"` or `"nearest"`.
/// Implicit `X = 1:cols(V)`, `Y = 1:rows(V)`, `Z = 1:pages(V)`.
/// Out-of-grid queries return `NaN`.
///
/// @param V       3-D value array.
/// @param Xq      Column-coordinate queries.
/// @param Yq      Row-coordinate queries.
/// @param Zq      Page-coordinate queries.
/// @param method  `"linear"` or `"nearest"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Interpolated values.
/// @see interp3(X, Y, Z, V, …, mr)
Value interp3(const Value &V, const Value &Xq, const Value &Yq, const Value &Zq,
              const std::string &method = "linear",
              std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D interpolation, explicit grid
/// (`Vq = interp3(X, Y, Z, V, Xq, Yq, Zq, method)`).
///
/// `X` / `Y` / `Z` are vectors giving column / row / page coordinates
/// (strictly monotonic ascending). Grid sizes must equal `cols(V)`,
/// `rows(V)`, `pages(V)` respectively.
///
/// @param X       Column coordinates.
/// @param Y       Row coordinates.
/// @param Z       Page coordinates.
/// @param V       3-D value array.
/// @param Xq      Column-coordinate queries.
/// @param Yq      Row-coordinate queries.
/// @param Zq      Page-coordinate queries.
/// @param method  `"linear"` or `"nearest"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Interpolated values.
Value interp3(const Value &X, const Value &Y, const Value &Z, const Value &V,
              const Value &Xq, const Value &Yq, const Value &Zq,
              const std::string &method = "linear",
              std::pmr::memory_resource *mr = nullptr);

/// @brief Natural cubic spline interpolation (`yq = spline(x, y, xq)`).
///
/// Equivalent to `interp1(x, y, xq, "spline")`.
///
/// @param x   Sample sites (strictly monotonic).
/// @param y   Sample values.
/// @param xq  Query points.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Interpolated values.
/// @see interp1, pchip
Value spline(const Value &x, const Value &y, const Value &xq,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Piecewise cubic Hermite interpolation (`yq = pchip(x, y, xq)`).
///
/// Equivalent to `interp1(x, y, xq, "pchip")`.
///
/// @param x   Sample sites (strictly monotonic).
/// @param y   Sample values.
/// @param xq  Query points.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Interpolated values.
/// @see interp1, spline
Value pchip(const Value &x, const Value &y, const Value &xq,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Akima cubic Hermite interpolation
/// (`yq = makima(x, y, xq)`).
///
/// Equivalent to `interp1(x, y, xq, "makima")`. The modified Akima
/// weight `|m_{i+1} - m_i| + |m_{i+1} + m_i| / 2` avoids the original
/// Akima method's zero-weight degeneracies on flat data.
///
/// KNOWN GAP: the 2-arg `pp = makima(x, y)` pp-form is not supported.
///
/// @param x   Sample sites (strictly monotonic).
/// @param y   Sample values.
/// @param xq  Query points.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Interpolated values.
/// @see interp1, pchip, spline
Value makima(const Value &x, const Value &y, const Value &xq,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Build a piecewise-polynomial (pp) struct (`pp = mkpp(breaks, coefs)`).
///
/// Fields: `{form='pp', breaks, coefs, pieces, order, dim}`.
/// `coefs` is `pieces × order`.
///
/// @param breaks  Break vector (length `pieces + 1`).
/// @param coefs   Coefficient matrix.
/// @param mr      Memory resource (nullptr → process default).
/// @return        pp-form struct.
/// @see ppval
Value mkpp(const Value &breaks, const Value &coefs,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Evaluate a piecewise polynomial (`y = ppval(pp, x)`).
///
/// Local Horner evaluation. Output shape mirrors `x`.
///
/// @param pp  pp-form struct (from @ref mkpp or `spline` outputs).
/// @param x   Evaluation points.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Values at `x`.
/// @see mkpp, spline
Value ppval(const Value &pp, const Value &x,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::math
