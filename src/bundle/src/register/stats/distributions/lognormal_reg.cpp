// toolboxes/signal/src/distributions/lognormal_reg.cpp
//
// CallContext register half of distributions/lognormal.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/math/random/rng.hpp>
#include <numkit/stats/distributions/lognormal.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "distributions/dist_helpers.hpp"
#include "distributions/lognormal_detail.hpp"
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

namespace {
inline double argMu(Span<const Value> args, size_t i) {
    return (args.size() > i && !args[i].isEmpty()) ? args[i].toScalar() : 0.0;
}
inline double argSigma(Span<const Value> args, size_t i) {
    return (args.size() > i && !args[i].isEmpty()) ? args[i].toScalar() : 1.0;
}
}

void lognpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lognpdf: requires (x[, mu, sigma])", 0, 0, "lognpdf", "", "numkit:lognpdf:nargin");
    auto *mr = ctx.engine->resource();
    Value hmu, hsig;
    const Value &mu  = dist_param(args, 1, 0.0, mr, hmu);
    const Value &sig = dist_param(args, 2, 1.0, mr, hsig);
    if (mu.isScalar() && sig.isScalar())
        outs[0] = lognpdf(args[0], mu.toScalar(), sig.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], mu, sig, mr, "lognpdf", lognpdfK);
}

void logncdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> s = args.subspan(0, stripUpperFlag(args, upper));
    if (s.empty())
        throw Error("logncdf: requires (x[, mu, sigma][, 'upper'])", 0, 0, "logncdf", "", "numkit:logncdf:nargin");
    auto *mr = ctx.engine->resource();
    Value hmu, hsig;
    const Value &mu  = dist_param(s, 1, 0.0, mr, hmu);
    const Value &sig = dist_param(s, 2, 1.0, mr, hsig);
    Value v = (mu.isScalar() && sig.isScalar())
                  ? logncdf(s[0], mu.toScalar(), sig.toScalar(), mr)
                  : broadcast_dist3(s[0], mu, sig, mr, "logncdf", logncdfK);
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void logninv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("logninv: requires (p[, mu, sigma])", 0, 0, "logninv", "", "numkit:logninv:nargin");
    auto *mr = ctx.engine->resource();
    Value hmu, hsig;
    const Value &mu  = dist_param(args, 1, 0.0, mr, hmu);
    const Value &sig = dist_param(args, 2, 1.0, mr, hsig);
    if (mu.isScalar() && sig.isScalar())
        outs[0] = logninv(args[0], mu.toScalar(), sig.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], mu, sig, mr, "logninv", logninvK);
}

void lognrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    const double mu    = argMu(args, 0);
    const double sigma = argSigma(args, 1);
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = lognrnd(ctx.engine->rng(), mu, sigma, rows, cols, ctx.engine->resource());
}

void lognstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "lognstat",
                       [](double mu, double sigma) { return lognstat(mu, sigma); });
}

} // namespace detail

} // namespace numkit::stats
