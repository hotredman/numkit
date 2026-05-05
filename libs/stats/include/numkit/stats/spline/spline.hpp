// libs/stats/include/numkit/stats/spline/spline.hpp
//
// Curve Fitting Toolbox — knot-manipulation primitives.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// `aveknt(t, k)` — knot averages (Greville sites).
///   ave_i = mean(t_{i+1}, …, t_{i+k-1}) for i = 1..length(t)-k.
Value aveknt(std::pmr::memory_resource *mr, const Value &t, int k);

/// `augknt(knots, k[, mults])` — augment knot sequence so each end
/// has multiplicity k. With a `mults` vector, every interior break
/// is repeated according to mults; the simple form repeats only
/// endpoints.
Value augknt(std::pmr::memory_resource *mr, const Value &knots, int k);

/// `brk2knt(breaks, mults)` — replicate each break by the matching
/// entry of `mults` and concatenate.
Value brk2knt(std::pmr::memory_resource *mr, const Value &breaks, const Value &mults);

/// `[breaks, mults] = knt2brk(knots)` — extract distinct values and
/// their multiplicities (preserving order, since the input is
/// non-decreasing).
std::tuple<Value, Value>
knt2brk(std::pmr::memory_resource *mr, const Value &knots);

/// `pp = ppmak(breaks, coefs[, d])` — piecewise-polynomial constructor.
/// `breaks` is a length-(L+1) vector; `coefs` is L×K (univariate, d=1)
/// or d·L×K (vector-valued). The struct carries fields {form='pp',
/// breaks, coefs, pieces=L, order=K, dim=d}.
Value ppmak(std::pmr::memory_resource *mr, const Value &breaks,
            const Value &coefs, int d);

/// `y = fnval(pp, x)` — evaluate a pp-form spline at x via Horner.
/// Handles only `form='pp'` for now; B-spline form is deferred.
Value fnval(std::pmr::memory_resource *mr, const Value &pp, const Value &x);

} // namespace numkit::stats
