// toolboxes/signal/src/distributions/hypergeom_reg.cpp
//
// CallContext register half of distributions/hypergeom.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/math/random/rng.hpp>
#include <numkit/stats/distributions/hypergeom.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "distributions/dist_helpers.hpp"
#include "distributions/hypergeom_detail.hpp"
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

// Helper: are all three params (args[i1..i1+2]) scalar?
inline bool hyge_params_scalar(Span<const Value> a, size_t i) {
    return a[i].isScalar() && a[i + 1].isScalar() && a[i + 2].isScalar();
}

void hygepdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("hygepdf: requires (k, M, K, N)", 0, 0, "hygepdf", "", "numkit:hygepdf:nargin");
    auto *mr = ctx.engine->resource();
    if (hyge_params_scalar(args, 1))
        outs[0] = hygepdf(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), mr);
    else
        outs[0] = broadcast_dist4(args[0], args[1], args[2], args[3], mr, "hygepdf", hygepdfK);
}

void hygecdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 4)
        throw Error("hygecdf: requires (k, M, K, N[, 'upper'])", 0, 0, "hygecdf", "", "numkit:hygecdf:nargin");
    auto *mr = ctx.engine->resource();
    Value v = hyge_params_scalar(a, 1)
                  ? hygecdf(a[0], a[1].toScalar(), a[2].toScalar(), a[3].toScalar(), mr)
                  : broadcast_dist4(a[0], a[1], a[2], a[3], mr, "hygecdf", hygecdfK);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void hygeinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("hygeinv: requires (q, M, K, N)", 0, 0, "hygeinv", "", "numkit:hygeinv:nargin");
    auto *mr = ctx.engine->resource();
    if (hyge_params_scalar(args, 1))
        outs[0] = hygeinv(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), mr);
    else
        outs[0] = broadcast_dist4(args[0], args[1], args[2], args[3], mr, "hygeinv", hygeinvK);
}

void hygernd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("hygernd: requires (M, K, N[, m, n])", 0, 0, "hygernd", "", "numkit:hygernd:nargin");
    const double M = args[0].toScalar();
    const double K = args[1].toScalar();
    const double N = args[2].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 4 && !args[3].isEmpty()) rows = static_cast<size_t>(args[3].toScalar());
    if (args.size() >= 5 && !args[4].isEmpty()) cols = static_cast<size_t>(args[4].toScalar());
    else if (args.size() >= 4) cols = rows;
    outs[0] = hygernd(ctx.engine->rng(), M, K, N, rows, cols, ctx.engine->resource());
}

void hygestat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_3arg(args, nargout, outs, ctx.engine->resource(), "hygestat",
                       [](double M, double K, double N) {
                           return hygestat(M, K, N);
                       });
}

} // namespace detail

} // namespace numkit::stats
