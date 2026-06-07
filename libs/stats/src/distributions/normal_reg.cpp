// libs/signal/src/distributions/normal_reg.cpp
//
// CallContext register half of distributions/normal.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>          // sharedEngine, randn
#include <numkit/stats/distributions/normal.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"            // stripUpperFlag / applyUpperInPlace
#include "normal_detail.hpp"
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

void normpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normpdf: requires at least 1 argument",
                     0, 0, "normpdf", "", "numkit:normpdf:nargin");
    auto *mr = ctx.engine->resource();
    Value hmu, hsig;
    const Value &mu  = dist_param(args, 1, 0.0, mr, hmu);
    const Value &sig = dist_param(args, 2, 1.0, mr, hsig);
    outs[0] = broadcast_dist3(args[0], mu, sig, mr, "normpdf", normpdfK);
}

void normcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normcdf: requires at least 1 argument",
                     0, 0, "normcdf", "", "numkit:normcdf:nargin");
    auto *mr = ctx.engine->resource();
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    Value hmu, hsig;
    const Value &mu  = dist_param(a, 1, 0.0, mr, hmu);
    const Value &sig = dist_param(a, 2, 1.0, mr, hsig);
    Value v = broadcast_dist3(a[0], mu, sig, mr, "normcdf", normcdfK);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void norminv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("norminv: requires at least 1 argument",
                     0, 0, "norminv", "", "numkit:norminv:nargin");
    auto *mr = ctx.engine->resource();
    Value hmu, hsig;
    const Value &mu  = dist_param(args, 1, 0.0, mr, hmu);
    const Value &sig = dist_param(args, 2, 1.0, mr, hsig);
    outs[0] = broadcast_dist3(args[0], mu, sig, mr, "norminv", norminvK);
}

void normrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("normrnd: requires (mu, sigma[, sz...])",
                     0, 0, "normrnd", "", "numkit:normrnd:nargin");
    const double mu = args[0].toScalar();
    const double sigma = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = normrnd(mu, sigma, rows, cols, ctx.engine->resource());
}

void normstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "normstat",
                       [](double mu, double sigma) { return normstat(mu, sigma); });
}

} // namespace detail

} // namespace numkit::stats
