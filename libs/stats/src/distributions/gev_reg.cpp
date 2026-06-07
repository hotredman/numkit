// libs/signal/src/distributions/gev_reg.cpp
//
// CallContext register half of distributions/gev.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/stats/distributions/gev.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"
#include "gev_detail.hpp"
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

void gevpdf_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("gevpdf: requires (x, k, sigma, mu)",
                    0, 0, "gevpdf", "", "numkit:gevpdf:nargin");
    outs[0] = gevpdf(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), ctx.engine->resource());
}

void gevcdf_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 4)
        throw Error("gevcdf: requires (x, k, sigma, mu[, 'upper'])",
                    0, 0, "gevcdf", "", "numkit:gevcdf:nargin");
    Value v = gevcdf(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void gevinv_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("gevinv: requires (p, k, sigma, mu)",
                    0, 0, "gevinv", "", "numkit:gevinv:nargin");
    outs[0] = gevinv(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), ctx.engine->resource());
}

void gevrnd_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gevrnd: requires (k, sigma, mu[, m, n])",
                    0, 0, "gevrnd", "", "numkit:gevrnd:nargin");
    const double k     = args[0].toScalar();
    const double sigma = args[1].toScalar();
    const double mu    = args[2].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 4 && !args[3].isEmpty()) rows = static_cast<size_t>(args[3].toScalar());
    if (args.size() >= 5 && !args[4].isEmpty()) cols = static_cast<size_t>(args[4].toScalar());
    else if (args.size() >= 4) cols = rows;
    outs[0] = gevrnd(k, sigma, mu, rows, cols, ctx.engine->resource());
}

void gevstat_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_3arg(args, nargout, outs, ctx.engine->resource(), "gevstat",
                       [](double k, double sigma, double mu) {
                           return gevstat(k, sigma, mu);
                       });
    return;
}

} // namespace detail

} // namespace numkit::stats
