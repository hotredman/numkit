// toolboxes/signal/src/distributions/geometric_reg.cpp
//
// CallContext register half of distributions/geometric.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/datafun.hpp>
#include <numkit/stats/distributions/geometric.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "distributions/dist_helpers.hpp"
#include "distributions/geometric_detail.hpp"
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

void geopdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("geopdf: requires (k, p)", 0, 0, "geopdf", "", "numkit:geopdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &p = args[1];
    if (p.isScalar())
        outs[0] = geopdf(args[0], p.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], p, mr, "geopdf", geopdfK);
}

void geocdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 2)
        throw Error("geocdf: requires (k, p[, 'upper'])", 0, 0, "geocdf", "", "numkit:geocdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &p = a[1];
    Value v = p.isScalar() ? geocdf(a[0], p.toScalar(), mr)
                           : broadcast_dist2(a[0], p, mr, "geocdf", geocdfK);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void geoinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("geoinv: requires (q, p)", 0, 0, "geoinv", "", "numkit:geoinv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &p = args[1];
    if (p.isScalar())
        outs[0] = geoinv(args[0], p.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], p, mr, "geoinv", geoinvK);
}

void geornd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("geornd: requires p[, m, n]", 0, 0, "geornd", "", "numkit:geornd:nargin");
    const double p = args[0].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 2 && !args[1].isEmpty()) rows = static_cast<size_t>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty()) cols = static_cast<size_t>(args[2].toScalar());
    else if (args.size() >= 2) cols = rows;
    outs[0] = geornd(ctx.engine->rng(), p, rows, cols, ctx.engine->resource());
}

void geostat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx.engine->resource(), "geostat",
                       [](double p) { return geostat(p); });
}

} // namespace detail

} // namespace numkit::stats
