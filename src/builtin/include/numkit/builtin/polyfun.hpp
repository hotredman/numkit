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

struct PolyDiv {
    Value q;
    Value r;
};

struct PadeCoef {
    Value num;
    Value den;
};

/// @brief Matrix polynomial evaluation (`polyvalm(p, A)`).
Value polyvalm(const Value &p, const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Characteristic polynomial of matrix (`poly_of_matrix(A)`).
Value poly_of_matrix(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Padé approximation of time delay (`[num, den] = padecoef(T, N)`).
PadeCoef padecoef(double T, int N, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial division (`[q, r] = polydiv(b, a)`).
PolyDiv polydiv(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Transfer function to zero-pole-gain (`[z, p, k] = tf2zp(b, a)`).
std::tuple<Value, Value, Value> tf2zp(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Zero-pole-gain to transfer function (`[b, a] = zp2tf(z, p, k)`).
std::tuple<Value, Value> zp2tf(const Value &z, const Value &p, double k, std::pmr::memory_resource *mr = nullptr);

struct ResidueResult {
    Value r;
    Value p;
    Value k;
};

/// @brief Partial fraction expansion (residues) in s-domain (`[r, p, k] = residue(b, a)`).
ResidueResult residue(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Partial fraction expansion in z-domain (`[r, p, k] = residuez(b, a)`).
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
Value interp2(const Value &V, const Value &Xq, const Value &Yq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);
Value interp2(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

Value interp3(const Value &V, const Value &Xq, const Value &Yq, const Value &Zq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);
Value interp3(const Value &X, const Value &Y, const Value &Z, const Value &V, const Value &Xq, const Value &Yq, const Value &Zq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

Value spline(const Value &x, const Value &y, const Value &xq = Value(), std::pmr::memory_resource *mr = nullptr);
Value pchip(const Value &x, const Value &y, const Value &xq = Value(), std::pmr::memory_resource *mr = nullptr);
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

Value gradient(const Value &f, double h = 1.0, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value> gradient2(const Value &f, double hx = 1.0, double hy = 1.0, std::pmr::memory_resource *mr = nullptr);
Value del2(const Value &u, double h = 1.0, std::pmr::memory_resource *mr = nullptr);

Value trapz(const Value &y, std::pmr::memory_resource *mr = nullptr);
Value trapz(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);
Value trapz(const Value &x, const Value &y, int dim, std::pmr::memory_resource *mr = nullptr);

Value cumtrapz(const Value &y, std::pmr::memory_resource *mr = nullptr);
Value cumtrapz(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);
Value cumtrapz(const Value &x, const Value &y, int dim, std::pmr::memory_resource *mr = nullptr);
Value cumtrapzDim(const Value &y, int dim, std::pmr::memory_resource *mr = nullptr);

Value integral(FnHandle fn, double a, double b, double absTol = 1e-10, std::pmr::memory_resource *mr = nullptr);
Value integral2(FnHandle fn, double a, double b, double c, double d, double absTol = 1e-10, std::pmr::memory_resource *mr = nullptr);
Value integral3(FnHandle fn, double a, double b, double c, double d, double e, double f, double absTol = 1e-10, std::pmr::memory_resource *mr = nullptr);

// ── Computational Geometry ──────────────────────────────────────────────────

struct MatchpairsResult {
    Value M;
    Value uR;
    Value uC;
};

struct InpolygonResult {
    Value in;
    Value on;
};

struct PolyxpolyResult {
    Value xi;
    Value yi;
    Value segments;
};

Value polyarea(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);
Value inpolygon(const Value &xq, const Value &yq, const Value &xv, const Value &yv, std::pmr::memory_resource *mr = nullptr);
InpolygonResult inpolygon2(const Value &xq, const Value &yq, const Value &xv, const Value &yv, std::pmr::memory_resource *mr = nullptr);
Value convhull(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);
PolyxpolyResult polyxpoly(const Value &x1, const Value &y1, const Value &x2, const Value &y2, std::pmr::memory_resource *mr = nullptr);
MatchpairsResult matchpairs(const Value &C, double cU, const std::string &mode = "min", std::pmr::memory_resource *mr = nullptr);
Value boundary(const Value &x, const Value &y, double shrink = 0.5, std::pmr::memory_resource *mr = nullptr);
Value delaunay(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);
Value griddata(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, std::pmr::memory_resource *mr = nullptr);
Value griddata(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, const std::string &method, std::pmr::memory_resource *mr = nullptr);
Value griddatan(const Value &Xv, const Value &vv, const Value &xi, std::pmr::memory_resource *mr = nullptr);
Value griddatan(const Value &Xv, const Value &vv, const Value &xi, const std::string &method, std::pmr::memory_resource *mr = nullptr);
Value histcounts2(const Value &x, const Value &y, const Value &xedgesV, const Value &yedgesV, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
