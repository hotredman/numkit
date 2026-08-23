/// @file spline.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/spline/spline.hpp
//
// Knot-manipulation primitives and pp-form splines.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

/// @brief Greville knot averages (`a = aveknt(t, k)`).
///
/// `a_i = mean(t_{i+1}, …, t_{i+k-1})` for `i = 1..length(t) - k`.
/// Useful for placing B-spline control sites along a knot vector.
///
/// @param t   Non-decreasing knot vector.
/// @param k   Spline order (`k >= 1`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row vector of length `length(t) - k`.
/// @see augknt
Value aveknt(const Value &t, int k, std::pmr::memory_resource *mr = nullptr);

/// @brief Augment knot sequence (`aug = augknt(knots, k)`).
///
/// Pads `knots` so each endpoint has multiplicity `k`. Used to build
/// B-spline open knot vectors.
///
/// @param knots  Interior knot vector (non-decreasing).
/// @param k      Spline order.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Augmented knot vector.
/// @see aveknt, brk2knt
Value augknt(const Value &knots, int k,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Replicate breaks by multiplicities (`t = brk2knt(breaks, mults)`).
///
/// Returns the concatenation where each `breaks(i)` appears `mults(i)`
/// times. Inverse of @ref knt2brk.
///
/// @param breaks  Distinct break values.
/// @param mults   Multiplicities (`length(mults) == length(breaks)`).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Expanded knot vector.
/// @see knt2brk
Value brk2knt(const Value &breaks, const Value &mults,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Decompose knot vector (`[breaks, mults] = knt2brk(knots)`).
///
/// Extracts distinct values and their multiplicities, preserving order
/// (input must be non-decreasing).
///
/// @param knots  Non-decreasing knot vector.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(breaks, mults)` pair.
/// @see brk2knt
std::tuple<Value, Value>
knt2brk(const Value &knots, std::pmr::memory_resource *mr = nullptr);

/// @brief Piecewise-polynomial constructor (`pp = ppmak(breaks, coefs, d)`).
///
/// Builds a struct with fields `{form='pp', breaks, coefs, pieces=L,
/// order=K, dim=d}`. `breaks` is length `L+1`; `coefs` is `L × K` for
/// univariate (`d = 1`) or `d·L × K` for vector-valued.
///
/// @param breaks  Knot break vector (length `L + 1`).
/// @param coefs   Coefficient matrix (`L × K` or `d·L × K`).
/// @param d       Output dimension (1 for univariate).
/// @param mr      Memory resource (nullptr → process default).
/// @return        pp-form spline struct.
/// @see fnval, fnbrk
Value ppmak(const Value &breaks, const Value &coefs, int d,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Evaluate a pp-form spline (`y = fnval(pp, x)`).
///
/// Horner-form evaluation. Only `form == "pp"` is supported here;
/// B-spline form is deferred to future work.
///
/// @param pp  pp-form spline produced by @ref ppmak / @ref csapi.
/// @param x   Evaluation points (any shape).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Spline values, same shape as `x`.
/// @see ppmak, fnder, fnint
Value fnval(const Value &pp, const Value &x,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Differentiate a pp-form spline (`dpp = fnder(pp, order)`).
///
/// Differentiates the spline `order` times (default 1). Result has
/// reduced polynomial order `K - order`.
///
/// @param pp     pp-form spline.
/// @param order  Differentiation order (`>= 1`, default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Derivative spline (pp form).
/// @see fnval, fnint
Value fnder(const Value &pp, int order,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Antiderivative of a pp-form spline (`ipp = fnint(pp)`).
///
/// Integration constant chosen so the integral evaluates to 0 at the
/// first break and is continuous at piece boundaries.
///
/// @param pp  pp-form spline.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Antiderivative spline (pp form).
/// @see fnder, fnval
Value fnint(const Value &pp, std::pmr::memory_resource *mr = nullptr);

/// @brief Not-a-knot cubic spline interpolation (`pp = csapi(x, y)`).
///
/// Returns a pp-form spline of order 4 that interpolates `(x, y)` and
/// is C² with continuous third derivatives at `x(2)` and `x(end-1)`.
///
/// @param x   Sample sites (strictly increasing).
/// @param y   Sample values (matching shape with `x`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    pp-form spline.
/// @see ppmak, fnval
Value csapi(const Value &x, const Value &y,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Extract a named field from a pp-form spline (`v = fnbrk(pp, part)`).
///
/// `part ∈ {"breaks", "coefs", "pieces"|"l", "order"|"k", "dim", "form"}`.
/// Interval-restriction form is not yet supported.
///
/// @param pp    pp-form spline.
/// @param part  Field name.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Requested field as a Value.
/// @throws Error  Unknown field name (`m:fnbrk:badPart`).
Value fnbrk(const Value &pp, const std::string &part,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Combine pp-form splines linearly
/// (`pp = fncmb(pp1, c1, pp2, c2)`).
///
/// - Two-arg form (`pp2.isEmpty()`): `c1 · pp1` (scalar multiply).
/// - Four-arg form: `c1 · pp1 + c2 · pp2` — requires `pp1` and `pp2`
///   to share breaks, order and dim.
///
/// @param pp1  First pp-form spline.
/// @param c1   Coefficient on `pp1`.
/// @param pp2  Second spline. Pass `Value::Empty` for the
///             scalar-multiply form.
/// @param c2   Coefficient on `pp2` (ignored when `pp2.isEmpty()`).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Combined pp-form spline.
/// @throws Error  Mismatched breaks / order / dim
///                (`m:fncmb:incompatible`).
Value fncmb(const Value &pp1, double c1,
            const Value &pp2 = Value::Empty, double c2 = 0.0,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
