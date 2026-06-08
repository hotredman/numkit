// toolboxes/signal/src/distributions/rician_reg.cpp
//
// CallContext register half of distributions/rician.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>   // besseli
#include <numkit/comm/channel/channel.hpp>           // marcumq
#include <numkit/stats/distributions/rician.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"
#include "rician_detail.hpp"
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

void ricepdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ricepdf: requires (x, s, sigma)",
                    0, 0, "ricepdf", "", "numkit:ricepdf:nargin");
    outs[0] = ricepdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void ricecdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("ricecdf: requires (x, s, sigma[, 'upper'])",
                    0, 0, "ricecdf", "", "numkit:ricecdf:nargin");
    Value v = ricecdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void riceinv_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("riceinv: requires (p, s, sigma)",
                    0, 0, "riceinv", "", "numkit:riceinv:nargin");
    outs[0] = riceinv(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void ricernd_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ricernd: requires (s, sigma[, m, n])",
                    0, 0, "ricernd", "", "numkit:ricernd:nargin");
    const double s     = args[0].toScalar();
    const double sigma = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = ricernd(s, sigma, rows, cols, ctx.engine->resource());
}

void ricestat_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "ricestat",
                       [mr](double s, double sigma) { return ricestat(s, sigma, mr); });
}

} // namespace detail

} // namespace numkit::stats
