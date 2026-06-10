// toolboxes/signal/src/distributions/gamma_dist_reg.cpp
//
// CallContext register half of distributions/gamma_dist.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>
#include <numkit/stats/distributions/gamma_dist.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"
#include "gamma_dist_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

void gampdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gampdf: requires (x, a, b)", 0, 0, "gampdf", "", "numkit:gampdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &a = args[1];
    const Value &b = args[2];
    if (a.isScalar() && b.isScalar())
        outs[0] = gampdf(args[0], a.toScalar(), b.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], a, b, mr, "gampdf", gampdfK);
}

void gamcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a0 = args.subspan(0, stripUpperFlag(args, upper));
    if (a0.size() < 3)
        throw Error("gamcdf: requires (x, a, b[, 'upper'])", 0, 0, "gamcdf", "", "numkit:gamcdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &a = a0[1];
    const Value &b = a0[2];
    Value v;
    if (a.isScalar() && b.isScalar()) {
        v = gamcdf(a0[0], a.toScalar(), b.toScalar(), mr);
    } else {
        // F(x; a, b) = gammainc(x/b clamped at 0, a). xs broadcasts (x, b)
        // (b<=0 → NaN), then gammainc broadcasts (xs, a) and gives NaN where
        // a<=0 — matching the scalar path's a<=0||b<=0 → NaN.
        Value xs = broadcast_dist2(a0[0], b, mr, "gamcdf", [](double xi, double bi) {
            if (!(bi > 0.0)) return std::numeric_limits<double>::quiet_NaN();
            return (xi <= 0.0) ? 0.0 : xi / bi;
        });
        v = ::numkit::math::gammainc(xs, a, mr);
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void gaminv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gaminv: requires (p, a, b)", 0, 0, "gaminv", "", "numkit:gaminv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &p = args[0];
    const Value &a = args[1];
    const Value &b = args[2];
    if (a.isScalar() && b.isScalar()) {
        outs[0] = gaminv(p, a.toScalar(), b.toScalar(), mr);   // unchanged fast path
        return;
    }
    // Broadcast: q = gammaincinv(p, a) (broadcasts (p, a)); then scale by b and
    // apply the degenerate quantile per element, matching the scalar gaminv:
    // a<0 or b<=0 → NaN; a==0 → 0 (p∈[0,1]) / NaN; else b·gammaincinv(p, a).
    const size_t np = p.numel(), na = a.numel(), nb = b.numel();
    if (np == 0 || na == 0 || nb == 0) {
        outs[0] = dist_empty_like(np == 0 ? p : (na == 0 ? a : b), mr);
        return;
    }
    const size_t N = dist_match_numel({np, na, nb}, "gaminv");
    Value q = ::numkit::math::gammaincinv(p, a, mr);
    const size_t nq = q.numel();
    const Value &ref = (np == N) ? p : (na == N ? a : b);
    Value out = dist_empty_like(ref, mr);
    double *od = out.doubleDataMut();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < N; ++i) {
        const double pi = p.elemAsDouble(np == 1 ? 0 : i);
        const double ai = a.elemAsDouble(na == 1 ? 0 : i);
        const double bi = b.elemAsDouble(nb == 1 ? 0 : i);
        const double qi = q.elemAsDouble(nq == 1 ? 0 : i);
        od[i] = (ai < 0.0 || !(bi > 0.0))
                    ? NaN
                    : (ai == 0.0 ? ((pi >= 0.0 && pi <= 1.0) ? 0.0 : NaN) : bi * qi);
    }
    outs[0] = std::move(out);
}

void gamrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gamrnd: requires (a, b[, sz...])", 0, 0, "gamrnd", "", "numkit:gamrnd:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = gamrnd(a, b, rows, cols, ctx.engine->resource());
}

void gamstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "gamstat",
                       [](double a, double b) { return gamstat(a, b); });
}

// ── randg — undocumented but very widely used "raw" gamma RNG ─────────
//
// `randg(shape, ...)` is shorthand for gamma(shape, 1) — i.e. scale = 1.
// Forms:
//   r = randg(shape)            — scalar (or per-element if shape is an
//                                  array)
//   r = randg(shape, n)         — n×n matrix
//   r = randg(shape, m, n)      — m×n matrix
//   r = randg(shape, [m n])     — same
//
// Implementation: delegate to gamrnd with b = 1. When shape is an array
// and no explicit size args are given, draw one sample per shape entry
// (each with its own shape parameter).
void randg_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = randg(1.0, 1, 1, mr);
        return;
    }
    const Value &shape = args[0];
    // Per-element form: array shape AND no extra size args.
    if (!shape.isScalar() && args.size() == 1) {
        outs[0] = randg(shape, mr);
        return;
    }
    // Scalar shape — pull size from the remaining args (or default 1×1).
    std::size_t rows = 1, cols = 1;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = randg(shape.toScalar(), rows, cols, mr);
}

} // namespace detail

} // namespace numkit::stats
