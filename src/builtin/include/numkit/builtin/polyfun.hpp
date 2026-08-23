/// @file polyfun.hpp
/// @ingroup group_polyfun
// src/builtin/include/numkit/builtin/polyfun.hpp
//
// Pure C++ Polynomials, interpolation, integration, and piecewise polynomials.
#pragma once

#include <functional>
#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/fn_handle.hpp>

namespace numkit::builtin {

using ::numkit::FnHandle;

/// @brief Computes polynomial roots (`roots(p)`).
/// @param p Vector of polynomial coefficients in descending powers: `p[0]*x^N + ... + p[N]`.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Column vector of complex/real roots (eigenvalues of companion matrix).
/// @see poly, polyval
Value roots(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial with specified roots or characteristic polynomial of a matrix.
/// @param r Vector of roots or square matrix.
/// @param mr Memory resource.
/// @return Row vector of polynomial coefficients in descending powers.
/// @see roots, polyval
Value poly(const Value &r, std::pmr::memory_resource *mr = nullptr);

/// @brief Evaluates polynomial `p` at points `x` via Horner's scheme.
/// @param p Vector of polynomial coefficients.
/// @param x Evaluation point(s).
/// @param mr Memory resource.
/// @return Evaluated values with the same shape as @p x.
/// @see roots, polyder, polyint
Value polyval(const Value &p, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes polynomial derivative (`polyder(p)`).
/// @param p Vector of polynomial coefficients.
/// @param mr Memory resource.
/// @return Derivative polynomial coefficients.
/// @see polyint, polyval
Value polyder(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes derivative of polynomial quotient `b / a` (`[q, d] = polyder(b, a)`).
/// @param b Numerator polynomial coefficients.
/// @param a Denominator polynomial coefficients.
/// @param mr Memory resource.
/// @return Tuple containing numerator and denominator polynomial coefficients `{q, d}`.
std::tuple<Value, Value> polyder(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes polynomial integral (`polyint(p, k)`).
/// @param p Vector of polynomial coefficients.
/// @param k Constant of integration (default: 0.0).
/// @param mr Memory resource.
/// @return Integrated polynomial coefficients.
/// @see polyder, polyval
Value polyint(const Value &p, double k = 0.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial least-squares curve fitting (`polyfit(x, y, n)`).
/// @param x Independent variable vector.
/// @param y Dependent variable vector.
/// @param n Degree of the fitting polynomial.
/// @param mr Memory resource.
/// @return Fitted polynomial coefficients of length `n + 1`.
Value polyfit(const Value &x, const Value &y, int n, std::pmr::memory_resource *mr = nullptr);

#include <tuple>

/// @brief Result of polynomial division and deconvolution (`[q, r] = polydiv(b, a)`).
struct PolyDiv {
    Value q; ///< Quotient polynomial coefficients.
    Value r; ///< Remainder polynomial coefficients.
};

/// @brief Padé approximation numerator and denominator polynomials.
struct PadeCoef {
    Value num; ///< Numerator polynomial coefficients.
    Value den; ///< Denominator polynomial coefficients.
};

/// @brief Matrix polynomial evaluation (`polyvalm(p, A)`).
///
/// Evaluates polynomial `p` at square matrix `A` using Horner's method with matrix multiplications:
/// `Y = p[0]*A^n + p[1]*A^(n-1) + ... + p[n]*I`.
///
/// @param p Coefficient vector in descending powers of s.
/// @param A Square matrix.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Square matrix result of polynomial evaluation.
/// @see polyval, poly_of_matrix
Value polyvalm(const Value &p, const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Characteristic polynomial of square matrix (`poly_of_matrix(A)`).
///
/// Computes coefficients of `det(s*I - A)`.
///
/// @param A Square matrix.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Row vector containing characteristic polynomial coefficients.
/// @see poly, roots, polyvalm
Value poly_of_matrix(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Padé approximation of time delay (`[num, den] = padecoef(T, N)`).
///
/// Computes order-N Padé rational approximation of time delay `exp(-s*T)`.
///
/// @param T Time delay in seconds.
/// @param N Approximation order.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return PadeCoef containing numerator and denominator polynomials.
PadeCoef padecoef(double T, int N, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial deconvolution and division (`[q, r] = polydiv(b, a)`).
///
/// @param b Dividend polynomial coefficients.
/// @param a Divisor polynomial coefficients.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return PolyDiv containing quotient `q` and remainder `r`.
/// @see deconv, conv
PolyDiv polydiv(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Transfer function to zero-pole-gain conversion (`[z, p, k] = tf2zp(b, a)`).
///
/// @param b Numerator polynomial coefficients.
/// @param a Denominator polynomial coefficients.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Tuple containing zeros `z`, poles `p`, and gain `k`.
/// @see zp2tf, residue
std::tuple<Value, Value, Value> tf2zp(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Zero-pole-gain to transfer function conversion (`[b, a] = zp2tf(z, p, k)`).
///
/// @param z Zeros vector.
/// @param p Poles vector.
/// @param k Scalar gain factor.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Tuple containing numerator `b` and denominator `a` polynomials.
/// @see tf2zp, residue
std::tuple<Value, Value> zp2tf(const Value &z, const Value &p, double k, std::pmr::memory_resource *mr = nullptr);

/// @brief Result of partial fraction expansion (residues, poles, direct term).
struct ResidueResult {
    Value r; ///< Vector of complex residues.
    Value p; ///< Vector of complex poles.
    Value k; ///< Vector of direct term polynomial coefficients.
};

/// @brief Partial fraction expansion (residues) in Laplace s-domain (`[r, p, k] = residue(b, a)`).
///
/// Converts transfer function `B(s)/A(s)` to partial fraction form:
/// `B(s)/A(s) = r(1)/(s-p(1)) + ... + r(n)/(s-p(n)) + k(s)`.
///
/// @param b Numerator polynomial coefficients in descending powers of s.
/// @param a Denominator polynomial coefficients in descending powers of s.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return ResidueResult containing residues `r`, poles `p`, and direct terms `k`.
/// @see residuez, poly, roots
ResidueResult residue(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Partial fraction expansion in Z-domain (`[r, p, k] = residuez(b, a)`).
///
/// Converts discrete transfer function `B(z)/A(z)` to partial fraction form in `z^-1`:
/// `B(z)/A(z) = r(1)/(1 - p(1)*z^-1) + ... + k(z^-1)`.
///
/// @param b Numerator polynomial coefficients in descending powers of z.
/// @param a Denominator polynomial coefficients in descending powers of z.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return ResidueResult containing residues `r`, poles `p`, and direct terms `k`.
/// @see residue, poly, roots
ResidueResult residuez(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

// ── Interpolation & Piecewise Polynomials ───────────────────────────────────

/// @brief 1-D table lookup and data interpolation (`interp1(x, v, xq, method)`).
/// @param x Sample grid points (strictly monotonic).
/// @param v Sample values.
/// @param xq Query points.
/// @param method Interpolation method: `"linear"`, `"nearest"`, `"next"`, `"previous"`, `"spline"`, `"pchip"`.
/// @param mr Memory resource.
/// @return Interpolated values at query points `xq`.
/// @see interp2, spline, pchip
Value interp1(const Value &x, const Value &v, const Value &xq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D data interpolation on a grid (`interp2(x, y, v, xq, yq, method)`).
/// @param x Grid coordinates along columns.
/// @param y Grid coordinates along rows.
/// @param v Matrix of values on grid.
/// @param xq Query points x-coordinates.
/// @param yq Query points y-coordinates.
/// @param method Interpolation method: `"linear"`, `"nearest"`, `"cubic"`, `"spline"`.
/// @param mr Memory resource.
/// @return Interpolated values.
/// @see interp1
/// @brief 2-D data interpolation on a uniform grid (`interp2(V, Xq, Yq, method)`).
/// @param V Matrix of values on default integer grid.
/// @param Xq Query points x-coordinates.
/// @param Yq Query points y-coordinates.
/// @param method Interpolation method (`"linear"`, `"nearest"`, `"cubic"`, `"spline"`).
/// @param mr Memory resource.
/// @return Interpolated values.
Value interp2(const Value &V, const Value &Xq, const Value &Yq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D data interpolation on a grid (`interp2(x, y, v, xq, yq, method)`).
/// @param x Grid coordinates along columns.
/// @param y Grid coordinates along rows.
/// @param v Matrix of values on grid.
/// @param xq Query points x-coordinates.
/// @param yq Query points y-coordinates.
/// @param method Interpolation method: `"linear"`, `"nearest"`, `"cubic"`, `"spline"`.
/// @param mr Memory resource.
/// @return Interpolated values.
/// @see interp1
Value interp2(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D data interpolation on a uniform grid (`interp3(V, Xq, Yq, Zq, method)`).
/// @param V 3-D array of values.
/// @param Xq Query points x-coordinates.
/// @param Yq Query points y-coordinates.
/// @param Zq Query points z-coordinates.
/// @param method Interpolation method.
/// @param mr Memory resource.
/// @return Interpolated 3-D array.
Value interp3(const Value &V, const Value &Xq, const Value &Yq, const Value &Zq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D data interpolation on a grid (`interp3(X, Y, Z, V, Xq, Yq, Zq, method)`).
/// @param X Grid coordinates along X.
/// @param Y Grid coordinates along Y.
/// @param Z Grid coordinates along Z.
/// @param V 3-D array of values.
/// @param Xq Query points x-coordinates.
/// @param Yq Query points y-coordinates.
/// @param Zq Query points z-coordinates.
/// @param method Interpolation method.
/// @param mr Memory resource.
/// @return Interpolated 3-D array.
Value interp3(const Value &X, const Value &Y, const Value &Z, const Value &V, const Value &Xq, const Value &Yq, const Value &Zq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// @brief Cubic spline data interpolation (`spline(x, y, xq)`).
/// @param x Sample grid points.
/// @param y Sample values.
/// @param xq Query points (or empty to return piecewise polynomial structure).
/// @param mr Memory resource.
/// @return Interpolated values or piecewise polynomial struct.
/// @see pchip, makima, ppval
Value spline(const Value &x, const Value &y, const Value &xq = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Piecewise Cubic Hermite Interpolating Polynomial (PCHIP) (`pchip(x, y, xq)`).
/// @param x Sample grid points.
/// @param y Sample values.
/// @param xq Query points (or empty to return piecewise polynomial structure).
/// @param mr Memory resource.
/// @return Shape-preserving interpolated values or piecewise polynomial struct.
/// @see spline, makima, ppval
Value pchip(const Value &x, const Value &y, const Value &xq = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Akima cubic Hermite interpolation (`makima(x, y, xq)`).
/// @param x Sample grid points.
/// @param y Sample values.
/// @param xq Query points.
/// @param mr Memory resource.
/// @return Interpolated values with reduced overshoots.
/// @see spline, pchip
Value makima(const Value &x, const Value &y, const Value &xq, std::pmr::memory_resource *mr = nullptr);

/// @brief Constructs a piecewise polynomial structure (ppform).
/// @param breaks Vector of break points.
/// @param coefs Matrix of local polynomial coefficients.
/// @param mr Memory resource.
/// @return Piecewise polynomial struct.
/// @see unmkpp, ppval
Value mkpp(const Value &breaks, const Value &coefs, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts piecewise polynomial structure fields (`[breaks, coefs, l, k, d] = unmkpp(pp)`).
/// @param pp Piecewise polynomial struct.
/// @param mr Memory resource.
/// @return Breaks vector from pp struct.
/// @see mkpp, ppval
Value unmkpp(const Value &pp, std::pmr::memory_resource *mr = nullptr);

/// @brief Evaluates a piecewise polynomial structure at query points `xq`.
/// @param pp Piecewise polynomial structure.
/// @param xq Query points.
/// @param mr Memory resource.
/// @return Evaluated values.
/// @see mkpp, unmkpp, spline, pchip
Value ppval(const Value &pp, const Value &xq, std::pmr::memory_resource *mr = nullptr);

/// @brief Numerical gradient of 1-D array or uniform multi-dimensional array (`gradient(f, h)`).
/// @param f Input array.
/// @param h Grid step size (default 1.0).
/// @param mr Memory resource.
/// @return Approximate derivative along the operating dimension.
/// @see del2, diff
Value gradient(const Value &f, double h = 1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Numerical 2-D gradient along rows and columns (`[fx, fy] = gradient(f, hx, hy)`).
/// @param f 2-D input matrix.
/// @param hx Grid spacing along X.
/// @param hy Grid spacing along Y.
/// @param mr Memory resource.
/// @return Tuple containing `{fx, fy}` partial derivatives.
std::tuple<Value, Value> gradient2(const Value &f, double hx = 1.0, double hy = 1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Five-point discrete Laplacian / Laplace operator (`del2(u, h)`).
/// @param u Input array.
/// @param h Spacing between grid points (default 1.0).
/// @param mr Memory resource.
/// @return Discrete Laplacian array.
/// @see gradient
Value del2(const Value &u, double h = 1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Trapezoidal numerical integration with unit spacing (`trapz(y)`).
/// @param y Function values vector or array.
/// @param mr Memory resource.
/// @return Integral approximation.
/// @see cumtrapz, integral
Value trapz(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Trapezoidal numerical integration over coordinates `x` (`trapz(x, y)`).
/// @param x Coordinate points.
/// @param y Function values.
/// @param mr Memory resource.
/// @return Integral approximation.
/// @see cumtrapz, integral
Value trapz(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Trapezoidal numerical integration along specified dimension (`trapz(x, y, dim)`).
/// @param x Coordinate points.
/// @param y Function values.
/// @param dim Dimension along which to integrate.
/// @param mr Memory resource.
/// @return Integrated array.
Value trapz(const Value &x, const Value &y, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal numerical integration with unit spacing (`cumtrapz(y)`).
/// @param y Function values.
/// @param mr Memory resource.
/// @return Cumulative integral array.
/// @see trapz
Value cumtrapz(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal numerical integration over coordinates `x` (`cumtrapz(x, y)`).
/// @param x Coordinate points.
/// @param y Function values.
/// @param mr Memory resource.
/// @return Cumulative integral array.
Value cumtrapz(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal numerical integration along dimension (`cumtrapz(x, y, dim)`).
/// @param x Coordinate points.
/// @param y Function values.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Cumulative integral array.
Value cumtrapz(const Value &x, const Value &y, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal numerical integration with unit spacing along dimension (`cumtrapz(y, dim)`).
/// @param y Function values.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Cumulative integral array.
Value cumtrapzDim(const Value &y, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Numerical 1-D adaptive quadrature integration (`integral(fn, a, b)`).
/// @param fn Function handle taking numeric vector argument.
/// @param a Lower integration limit.
/// @param b Upper integration limit.
/// @param absTol Absolute error tolerance.
/// @param mr Memory resource.
/// @return Integral value.
/// @see integral2, integral3, trapz
Value integral(FnHandle fn, double a, double b, double absTol = 1e-10, std::pmr::memory_resource *mr = nullptr);

/// @brief Numerical 2-D double integration over rectangular region (`integral2(fn, a, b, c, d)`).
/// @param fn Function handle taking `(x, y)`.
/// @param a Lower limit in x.
/// @param b Upper limit in x.
/// @param c Lower limit in y.
/// @param d Upper limit in y.
/// @param absTol Absolute error tolerance.
/// @param mr Memory resource.
/// @return Double integral value.
/// @see integral, integral3
Value integral2(FnHandle fn, double a, double b, double c, double d, double absTol = 1e-10, std::pmr::memory_resource *mr = nullptr);

/// @brief Numerical 3-D triple integration over cuboid region (`integral3(fn, a, b, c, d, e, f)`).
/// @param fn Function handle taking `(x, y, z)`.
/// @param a Lower limit in x.
/// @param b Upper limit in x.
/// @param c Lower limit in y.
/// @param d Upper limit in y.
/// @param e Lower limit in z.
/// @param f Upper limit in z.
/// @param absTol Absolute error tolerance.
/// @param mr Memory resource.
/// @return Triple integral value.
/// @see integral, integral2
Value integral3(FnHandle fn, double a, double b, double c, double d, double e, double f, double absTol = 1e-10, std::pmr::memory_resource *mr = nullptr);

// ── Computational Geometry ──────────────────────────────────────────────────

/// @brief Result of linear assignment problem (`matchpairs`).
struct MatchpairsResult {
    Value M;   ///< Matched index pairs `[row, col]`
    Value uR;  ///< Unmatched row indices
    Value uC;  ///< Unmatched column indices
};

/// @brief Result of 2-D polygon inclusion test (`inpolygon`).
struct InpolygonResult {
    Value in;  ///< Logical array of points inside polygon
    Value on;  ///< Logical array of points on polygon boundary
};

/// @brief Result of polygon intersection (`polyxpoly`).
struct PolyxpolyResult {
    Value xi;        ///< Intersection x-coordinates
    Value yi;        ///< Intersection y-coordinates
    Value segments;  ///< Intersection line segments
};

/// @brief Area of 2-D polygonal contour (`polyarea(x, y)`).
/// @param x Vertex x-coordinates.
/// @param y Vertex y-coordinates.
/// @param mr Memory resource.
/// @return Signed or unsigned polygon area.
Value polyarea(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if 2-D query points are inside polygonal region (`inpolygon(xq, yq, xv, yv)`).
/// @param xq Query points x-coordinates.
/// @param yq Query points y-coordinates.
/// @param xv Polygon vertices x-coordinates.
/// @param yv Polygon vertices y-coordinates.
/// @param mr Memory resource.
/// @return Logical array indicating whether query points lie inside or on edge.
/// @see inpolygon2
Value inpolygon(const Value &xq, const Value &yq, const Value &xv, const Value &yv, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests points inside and on edge of 2-D polygonal region (`[in, on] = inpolygon(xq, yq, xv, yv)`).
/// @param xq Query points x-coordinates.
/// @param yq Query points y-coordinates.
/// @param xv Polygon vertices x-coordinates.
/// @param yv Polygon vertices y-coordinates.
/// @param mr Memory resource.
/// @return Struct containing `{in, on}` logical arrays.
InpolygonResult inpolygon2(const Value &xq, const Value &yq, const Value &xv, const Value &yv, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes 2-D convex hull of planar point set (`convhull(x, y)`).
/// @param x Point x-coordinates.
/// @param y Point y-coordinates.
/// @param mr Memory resource.
/// @return Indices of vertices forming the convex hull in counterclockwise order.
/// @see delaunay, boundary
Value convhull(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Intersection points of polygonal lines or boundaries (`[xi, yi] = polyxpoly(x1, y1, x2, y2)`).
/// @param x1 First line x-coordinates.
/// @param y1 First line y-coordinates.
/// @param x2 Second line x-coordinates.
/// @param y2 Second line y-coordinates.
/// @param mr Memory resource.
/// @return Intersection coordinates struct.
PolyxpolyResult polyxpoly(const Value &x1, const Value &y1, const Value &x2, const Value &y2, std::pmr::memory_resource *mr = nullptr);

/// @brief Solves linear sum assignment problem with cost threshold (`matchpairs(C, cU, mode)`).
/// @param C Cost matrix.
/// @param cU Unmatched cost threshold.
/// @param mode Optimization mode (`"min"` or `"max"`).
/// @param mr Memory resource.
/// @return Matchpairs result containing matched indices and unmatched rows/cols.
MatchpairsResult matchpairs(const Value &C, double cU, const std::string &mode = "min", std::pmr::memory_resource *mr = nullptr);

/// @brief Computes boundary / alpha-shape around 2-D point cloud (`boundary(x, y, shrink)`).
/// @param x Point x-coordinates.
/// @param y Point y-coordinates.
/// @param shrink Shrink factor between 0.0 (convex hull) and 1.0 (compact boundary).
/// @param mr Memory resource.
/// @return Indices of boundary points.
/// @see convhull
Value boundary(const Value &x, const Value &y, double shrink = 0.5, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D Delaunay triangulation of planar point set (`delaunay(x, y)`).
/// @param x Point x-coordinates.
/// @param y Point y-coordinates.
/// @param mr Memory resource.
/// @return `M x 3` matrix where each row defines a triangle by point indices.
/// @see convhull
Value delaunay(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Interpolates scattered 2-D data onto query points (`griddata(x, y, v, xq, yq, method)`).
/// @param x Scattered point x-coordinates.
/// @param y Scattered point y-coordinates.
/// @param v Sample values at scattered points.
/// @param xq Query point x-coordinates.
/// @param yq Query point y-coordinates.
/// @param mr Memory resource.
/// @return Interpolated surface values.
/// @see griddatan, interp2
Value griddata(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, std::pmr::memory_resource *mr = nullptr);

/// @brief Interpolates scattered 2-D data onto query points with specified method (`griddata(x, y, v, xq, yq, method)`).
/// @param x Scattered point x-coordinates.
/// @param y Scattered point y-coordinates.
/// @param v Sample values at scattered points.
/// @param xq Query point x-coordinates.
/// @param yq Query point y-coordinates.
/// @param method Interpolation method (`"linear"`, `"nearest"`, `"cubic"`, `"natural"`).
/// @param mr Memory resource.
/// @return Interpolated surface values.
Value griddata(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, const std::string &method, std::pmr::memory_resource *mr = nullptr);

/// @brief N-D scattered data interpolation (`griddatan(Xv, vv, xi, method)`).
/// @param Xv `M x N` matrix of scattered sample point coordinates.
/// @param vv `M x 1` sample values.
/// @param xi `P x N` matrix of query point coordinates.
/// @param mr Memory resource.
/// @return Interpolated values vector `P x 1`.
/// @see griddata
Value griddatan(const Value &Xv, const Value &vv, const Value &xi, std::pmr::memory_resource *mr = nullptr);

/// @brief N-D scattered data interpolation with method (`griddatan(Xv, vv, xi, method)`).
/// @param Xv `M x N` sample point coordinates.
/// @param vv Sample values.
/// @param xi Query point coordinates.
/// @param method Interpolation method (`"linear"`, `"nearest"`).
/// @param mr Memory resource.
/// @return Interpolated values.
Value griddatan(const Value &Xv, const Value &vv, const Value &xi, const std::string &method, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes 2-D bivariate histogram bin counts (`histcounts2(x, y, xedges, yedges)`).
/// @param x Data points x-coordinates.
/// @param y Data points y-coordinates.
/// @param xedgesV Bin edges vector along X.
/// @param yedgesV Bin edges vector along Y.
/// @param mr Memory resource.
/// @return 2-D matrix of bivariate count values.
Value histcounts2(const Value &x, const Value &y, const Value &xedgesV, const Value &yedgesV, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
