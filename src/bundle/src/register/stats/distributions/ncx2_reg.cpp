// toolboxes/signal/src/distributions/ncx2_reg.cpp
//
// CallContext register half of distributions/ncx2.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/math/random/rng.hpp>
#include <numkit/math/special/special.hpp>   // besseli, gammainc
#include <numkit/stats/distributions/ncx2.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "distributions/dist_helpers.hpp"
#include "distributions/ncx2_detail.hpp"
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

void ncx2pdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ncx2pdf: requires (x, k, lambda)",
                    0, 0, "ncx2pdf", "", "numkit:ncx2pdf:nargin");
    outs[0] = ncx2pdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void ncx2cdf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("ncx2cdf: requires (x, k, lambda[, 'upper'])",
                    0, 0, "ncx2cdf", "", "numkit:ncx2cdf:nargin");
    Value v = ncx2cdf(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void ncx2inv_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ncx2inv: requires (p, k, lambda)",
                    0, 0, "ncx2inv", "", "numkit:ncx2inv:nargin");
    outs[0] = ncx2inv(args[0], args[1].toScalar(), args[2].toScalar(), ctx.engine->resource());
}

void ncx2rnd_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ncx2rnd: requires (k, lambda[, m, n])",
                    0, 0, "ncx2rnd", "", "numkit:ncx2rnd:nargin");
    const double k      = args[0].toScalar();
    const double lambda = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = ncx2rnd(ctx.engine->rng(), k, lambda, rows, cols, ctx.engine->resource());
}

void ncx2stat_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "ncx2stat",
                       [](double k, double lambda) { return ncx2stat(k, lambda); });
}

} // namespace detail

} // namespace numkit::stats
