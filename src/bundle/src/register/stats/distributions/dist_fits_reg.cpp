// toolboxes/signal/src/distributions/dist_fits_reg.cpp
//
// CallContext register half of distributions/dist_fits.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/builtin/specfun.hpp>   // gammainc (used by gamfit_full)
#include <nk/stats/distributions/beta.hpp>
#include <nk/stats/distributions/extreme_value.hpp>
#include <nk/stats/distributions/gamma_dist.hpp>
#include <nk/stats/distributions/gev.hpp>
#include <nk/stats/distributions/gp.hpp>
#include <nk/stats/distributions/negbin.hpp>
#include <nk/stats/distributions/weibull.hpp>
#include <nk/value/error.hpp>
#include <nk/value/value.hpp>
#include "distributions/dist_fits_detail.hpp"
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nk::stats {

namespace detail {

namespace {
// Helper: read optional `alpha` (2nd positional arg), default 0.05.
double parse_alpha(Span<const Value> args, std::size_t idx = 1)
{
    if (args.size() > idx) {
        const Value &v = args[idx];
        if (!v.isChar() && !v.isString() && !v.isEmpty())
            return v.toScalar();
    }
    return 0.05;
}
} // anonymous

void gamfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gamfit: requires (x[, alpha[, cens[, freq[, options]]]])",
                    0, 0, "gamfit", "", "nk:gamfit:nargin");
    auto *mr = ctx.engine->resource();
    const Value cens = (args.size() > 2) ? args[2] : Value::Empty;
    const Value freq = (args.size() > 3) ? args[3] : Value::Empty;
    outs[0] = gamfit(args[0], cens, freq, mr);
    if (nargout >= 2)
        outs[1] = gamfit_ci(args[0], parse_alpha(args), cens, freq, mr);
}

void wblfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wblfit: requires (x[, alpha[, cens[, freq[, options]]]])",
                    0, 0, "wblfit", "", "nk:wblfit:nargin");
    auto *mr = ctx.engine->resource();
    const Value cens = (args.size() > 2) ? args[2] : Value::Empty;
    const Value freq = (args.size() > 3) ? args[3] : Value::Empty;
    outs[0] = wblfit(args[0], cens, freq, mr);
    if (nargout >= 2)
        outs[1] = wblfit_ci(args[0], parse_alpha(args), cens, freq, mr);
}

void betafit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("betafit: requires (x[, alpha])",
                    0, 0, "betafit", "", "nk:betafit:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = betafit(args[0], mr);
    if (nargout >= 2) outs[1] = betafit_ci(args[0], parse_alpha(args), mr);
}

void nbinfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nbinfit: requires (x[, alpha])",
                    0, 0, "nbinfit", "", "nk:nbinfit:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = nbinfit(args[0], mr);
    if (nargout >= 2) outs[1] = nbinfit_ci(args[0], parse_alpha(args), mr);
}

void evfit_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("evfit: requires (x[, alpha[, cens[, freq[, options]]]])",
                    0, 0, "evfit", "", "nk:evfit:nargin");
    auto *mr = ctx.engine->resource();
    const Value cens = (args.size() > 2) ? args[2] : Value::Empty;
    const Value freq = (args.size() > 3) ? args[3] : Value::Empty;
    // args[4] = options struct — currently no-op (parsed for compat).
    outs[0] = evfit(args[0], cens, freq, mr);
    if (nargout >= 2)
        outs[1] = evfit_ci(args[0], parse_alpha(args), cens, freq, mr);
}

void gpfit_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gpfit: requires (x[, alpha])",
                    0, 0, "gpfit", "", "nk:gpfit:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = gpfit(args[0], mr);
    if (nargout >= 2) outs[1] = gpfit_ci(args[0], parse_alpha(args), mr);
}

void gevfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gevfit: requires (x[, alpha])",
                    0, 0, "gevfit", "", "nk:gevfit:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = gevfit(args[0], mr);
    if (nargout >= 2) outs[1] = gevfit_ci(args[0], parse_alpha(args), mr);
}

} // namespace detail

} // namespace nk::stats
