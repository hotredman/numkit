// toolboxes/signal/src/distributions/extreme_value_reg.cpp
//
// CallContext register half of distributions/extreme_value.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/datafun.hpp>
#include <numkit/stats/distributions/extreme_value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "distributions/dist_helpers.hpp"
#include "distributions/extreme_value_detail.hpp"
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

void evpdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("evpdf: requires x[, mu, sigma]",
                    0, 0, "evpdf", "", "numkit:evpdf:nargin");
    const double mu    = (args.size() >= 2) ? args[1].toScalar() : 0.0;
    const double sigma = (args.size() >= 3) ? args[2].toScalar() : 1.0;
    outs[0] = evpdf(args[0], mu, sigma, ctx.engine->resource());
}

void evcdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 1)
        throw Error("evcdf: requires x[, mu, sigma][, 'upper']",
                    0, 0, "evcdf", "", "numkit:evcdf:nargin");
    const double mu    = (n >= 2) ? args[1].toScalar() : 0.0;
    const double sigma = (n >= 3) ? args[2].toScalar() : 1.0;
    Value v = evcdf(args[0], mu, sigma, ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void evinv_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("evinv: requires p[, mu, sigma]",
                    0, 0, "evinv", "", "numkit:evinv:nargin");
    const double mu    = (args.size() >= 2) ? args[1].toScalar() : 0.0;
    const double sigma = (args.size() >= 3) ? args[2].toScalar() : 1.0;
    outs[0] = evinv(args[0], mu, sigma, ctx.engine->resource());
}

void evrnd_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("evrnd: requires (mu, sigma[, m, n])",
                    0, 0, "evrnd", "", "numkit:evrnd:nargin");
    const double mu    = args[0].toScalar();
    const double sigma = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = evrnd(ctx.engine->rng(), mu, sigma, rows, cols, ctx.engine->resource());
}

void evstat_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "evstat",
                       [](double mu, double sigma) { return evstat(mu, sigma); });
}

} // namespace detail

} // namespace numkit::stats
