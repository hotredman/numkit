// toolboxes/signal/src/distributions/gp_reg.cpp
//
// CallContext register half of distributions/gp.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/math/random/rng.hpp>
#include <numkit/stats/distributions/gp.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "distributions/dist_helpers.hpp"
#include "distributions/gp_detail.hpp"
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

void gppdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("gppdf: requires (x, k, sigma, theta)",
                    0, 0, "gppdf", "", "numkit:gppdf:nargin");
    outs[0] = gppdf(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), ctx.engine->resource());
}

void gpcdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 4)
        throw Error("gpcdf: requires (x, k, sigma, theta[, 'upper'])",
                    0, 0, "gpcdf", "", "numkit:gpcdf:nargin");
    Value v = gpcdf(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void gpinv_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("gpinv: requires (p, k, sigma, theta)",
                    0, 0, "gpinv", "", "numkit:gpinv:nargin");
    outs[0] = gpinv(args[0], args[1].toScalar(), args[2].toScalar(), args[3].toScalar(), ctx.engine->resource());
}

void gprnd_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gprnd: requires (k, sigma, theta[, m, n])",
                    0, 0, "gprnd", "", "numkit:gprnd:nargin");
    const double k     = args[0].toScalar();
    const double sigma = args[1].toScalar();
    const double theta = args[2].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 4 && !args[3].isEmpty()) rows = static_cast<size_t>(args[3].toScalar());
    if (args.size() >= 5 && !args[4].isEmpty()) cols = static_cast<size_t>(args[4].toScalar());
    else if (args.size() >= 4) cols = rows;
    outs[0] = gprnd(ctx.engine->rng(), k, sigma, theta, rows, cols, ctx.engine->resource());
}

void gpstat_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_3arg(args, nargout, outs, ctx.engine->resource(), "gpstat",
                       [](double k, double sigma, double theta) {
                           return gpstat(k, sigma, theta);
                       });
}

} // namespace detail

} // namespace numkit::stats
