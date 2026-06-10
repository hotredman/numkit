// toolboxes/signal/src/distributions/beta_reg.cpp
//
// CallContext register half of distributions/beta.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>
#include <numkit/stats/distributions/beta.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "beta_detail.hpp"
#include "dist_helpers.hpp"
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

void betapdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("betapdf: requires (x, a, b)", 0, 0, "betapdf", "", "numkit:betapdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &a = args[1];
    const Value &b = args[2];
    if (a.isScalar() && b.isScalar())
        outs[0] = betapdf(args[0], a.toScalar(), b.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], a, b, mr, "betapdf", betapdfK);
}

void betacdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a0 = args.subspan(0, stripUpperFlag(args, upper));
    if (a0.size() < 3)
        throw Error("betacdf: requires (x, a, b[, 'upper'])", 0, 0, "betacdf", "", "numkit:betacdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &a = a0[1];
    const Value &b = a0[2];
    Value v;
    if (a.isScalar() && b.isScalar()) {
        v = betacdf(a0[0], a.toScalar(), b.toScalar(), mr);
    } else {
        // F(x; a, b) = betainc(clamp01(x), a, b); betainc broadcasts (xc, a, b)
        // and betaincScalar gives NaN where a<=0 or b<=0. betainc does NOT
        // validate sizes (would OOB), so guard empties + size clash first.
        const size_t nx = a0[0].numel(), na = a.numel(), nb = b.numel();
        if (nx == 0 || na == 0 || nb == 0) {
            v = dist_empty_like(nx == 0 ? a0[0] : (na == 0 ? a : b), mr);
        } else {
            dist_match_numel({nx, na, nb}, "betacdf");
            Value xc = elementwise(a0[0], [](double xi) {
                if (xi <= 0.0) return 0.0;
                if (xi >= 1.0) return 1.0;
                return xi;
            }, mr);
            v = ::numkit::math::betainc(xc, a, b, mr);
        }
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void betainv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("betainv: requires (p, a, b)", 0, 0, "betainv", "", "numkit:betainv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &p = args[0];
    const Value &a = args[1];
    const Value &b = args[2];
    if (a.isScalar() && b.isScalar()) {
        outs[0] = betainv(p, a.toScalar(), b.toScalar(), mr);   // unchanged fast path
        return;
    }
    // betaincinv broadcasts (p, a, b) and gives NaN for a<=0||b<=0 (no other
    // degenerate). It does NOT validate sizes (would OOB), so guard first.
    const size_t np = p.numel(), na = a.numel(), nb = b.numel();
    if (np == 0 || na == 0 || nb == 0) {
        outs[0] = dist_empty_like(np == 0 ? p : (na == 0 ? a : b), mr);
        return;
    }
    dist_match_numel({np, na, nb}, "betainv");
    outs[0] = ::numkit::math::betaincinv(p, a, b, mr);
}

void betarnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("betarnd: requires (a, b[, sz...])", 0, 0, "betarnd", "", "numkit:betarnd:nargin");
    const double a = args[0].toScalar();
    const double b = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = betarnd(a, b, rows, cols, ctx.engine->resource());
}

void betastat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "betastat",
                       [](double a, double b) { return betastat(a, b); });
}

} // namespace detail

} // namespace numkit::stats
