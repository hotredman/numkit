// libs/signal/src/distributions/nakagami_reg.cpp
//
// CallContext register half of distributions/nakagami.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>   // gammainc, gammaincinv
#include <numkit/stats/distributions/nakagami.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"
#include "nakagami_detail.hpp"
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

void nakapdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nakapdf: requires (x, mu, omega)",
                    0, 0, "nakapdf", "", "numkit:nakapdf:nargin");
    outs[0] = nakapdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void nakacdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("nakacdf: requires (x, mu, omega[, 'upper'])",
                    0, 0, "nakacdf", "", "numkit:nakacdf:nargin");
    Value v = nakacdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void nakainv_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nakainv: requires (p, mu, omega)",
                    0, 0, "nakainv", "", "numkit:nakainv:nargin");
    outs[0] = nakainv(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void nakarnd_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nakarnd: requires (mu, omega[, m, n])",
                    0, 0, "nakarnd", "", "numkit:nakarnd:nargin");
    const double mu    = args[0].toScalar();
    const double omega = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = nakarnd(mu, omega, rows, cols, ctx.engine->resource());
}

void nakastat_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "nakastat",
                       [](double mu, double omega) { return nakastat(mu, omega); });
}

} // namespace detail

} // namespace numkit::stats
