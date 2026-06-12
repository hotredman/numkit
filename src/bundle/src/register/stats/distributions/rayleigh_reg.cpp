// toolboxes/signal/src/distributions/rayleigh_reg.cpp
//
// CallContext register half of distributions/rayleigh.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/math/random/rng.hpp>
#include <numkit/stats/distributions/rayleigh.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "distributions/dist_helpers.hpp"
#include "distributions/rayleigh_detail.hpp"
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

void raylpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("raylpdf: requires (x, b)", 0, 0, "raylpdf", "", "numkit:raylpdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &b = args[1];
    if (b.isScalar())
        outs[0] = raylpdf(args[0], b.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], b, mr, "raylpdf", raylpdfK);
}

void raylcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 2)
        throw Error("raylcdf: requires (x, b[, 'upper'])", 0, 0, "raylcdf", "", "numkit:raylcdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &b = a[1];
    Value v = b.isScalar() ? raylcdf(a[0], b.toScalar(), mr)
                           : broadcast_dist2(a[0], b, mr, "raylcdf", raylcdfK);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void raylinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("raylinv: requires (p, b)", 0, 0, "raylinv", "", "numkit:raylinv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &b = args[1];
    if (b.isScalar())
        outs[0] = raylinv(args[0], b.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], b, mr, "raylinv", raylinvK);
}

void raylrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("raylrnd: requires b[, sz...]", 0, 0, "raylrnd", "", "numkit:raylrnd:nargin");
    const double b = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = raylrnd(ctx.engine->rng(), b, rows, cols, ctx.engine->resource());
}

void raylstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx.engine->resource(), "raylstat",
                       [](double b) { return raylstat(b); });
}

} // namespace detail

} // namespace numkit::stats
