// libs/stats/include/numkit/stats/spline/spline.hpp
//
// Curve Fitting Toolbox — knot-manipulation primitives.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

/// `aveknt(t, k)` — knot averages (Greville sites).
///   ave_i = mean(t_{i+1}, …, t_{i+k-1}) for i = 1..length(t)-k.
Value aveknt(const Value &t, int k, std::pmr::memory_resource *mr = nullptr);

/// `augknt(knots, k[, mults])` — augment knot sequence so each end
/// has multiplicity k. With a `mults` vector, every interior break
/// is repeated according to mults; the simple form repeats only
/// endpoints.
Value augknt(const Value &knots, int k, std::pmr::memory_resource *mr = nullptr);

/// `brk2knt(breaks, mults)` — replicate each break by the matching
/// entry of `mults` and concatenate.
Value brk2knt(const Value &breaks, const Value &mults, std::pmr::memory_resource *mr = nullptr);

/// `[breaks, mults] = knt2brk(knots)` — extract distinct values and
/// their multiplicities (preserving order, since the input is
/// non-decreasing).
std::tuple<Value, Value>
knt2brk(const Value &knots, std::pmr::memory_resource *mr = nullptr);

/// `pp = ppmak(breaks, coefs[, d])` — piecewise-polynomial constructor.
/// `breaks` is a length-(L+1) vector; `coefs` is L×K (univariate, d=1)
/// or d·L×K (vector-valued). The struct carries fields {form='pp',
/// breaks, coefs, pieces=L, order=K, dim=d}.
Value ppmak(const Value &breaks, const Value &coefs, int d, std::pmr::memory_resource *mr = nullptr);

/// `y = fnval(pp, x)` — evaluate a pp-form spline at x via Horner.
/// Handles only `form='pp'` for now; B-spline form is deferred.
Value fnval(const Value &pp, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// `dpp = fnder(pp[, order])` — differentiate a pp-form spline `order`
/// times (default 1). Result has order = K − order.
Value fnder(const Value &pp, int order, std::pmr::memory_resource *mr = nullptr);

/// `ipp = fnint(pp)` — antiderivative of a pp-form spline, with
/// integration constant chosen so that the integral evaluates to 0
/// at the first break and is continuous at piece boundaries.
Value fnint(const Value &pp, std::pmr::memory_resource *mr = nullptr);

/// `pp = csapi(x, y)` — not-a-knot cubic spline interpolation.
/// Returns a pp-form spline of order 4 that interpolates (x, y) and
/// is C² with continuous third derivatives at x(2) and x(end-1).
Value csapi(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// `out = fnbrk(pp, part)` — extract a named part from a pp-form
/// spline: 'breaks', 'coefs', 'pieces' (or 'l'), 'order' (or 'k'),
/// 'dim', 'form'. Interval-restriction form is not yet supported.
Value fnbrk(const Value &pp, const std::string &part, std::pmr::memory_resource *mr = nullptr);

/// `pp = fncmb(pp1, c)`            — c·pp1 (scalar multiply).
/// `pp = fncmb(pp1, c1, pp2, c2)`  — c1·pp1 + c2·pp2 on shared breaks.
/// `pp2` may be null (unused) for the scalar-multiply form. The 4-arg
/// form requires pp1 and pp2 to share breaks / order / dim.
Value fncmb(const Value &pp1, double c1, const Value *pp2, double c2, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
