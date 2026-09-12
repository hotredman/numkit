// toolboxes/signal/src/regress/lasso_reg.cpp
//
// CallContext register half of regress/lasso.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/stats/regress/regress.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/value.hpp>
#include "regress/lasso_detail.hpp"
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

void lasso_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("lasso: requires (X, y, lambdas [, alpha])",
                    0, 0, "lasso", "", "nk:lasso:nargin");
    double alpha = 1.0;
    if (args.size() >= 4 && !args[3].isEmpty())
        alpha = args[3].toScalar();
    auto r = lasso(args[0], args[1], args[2], alpha, ctx.engine->resource());
    outs[0] = std::move(r.B);
    if (nargout > 1) outs[1] = std::move(r.Intercept);
    if (nargout > 2) outs[2] = std::move(r.Lambda);
}

void lassoglm_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("lassoglm: requires (X, y, distr, lambdas [, alpha])",
                    0, 0, "lassoglm", "", "nk:lassoglm:nargin");
    if (!args[2].isChar())
        throw Error("lassoglm: distr must be a string",
                    0, 0, "lassoglm", "", "nk:lassoglm:badDistr");
    const std::string s = args[2].toString();
    GlmDistribution d;
    if (s == "normal")        d = GlmDistribution::Normal;
    else if (s == "binomial") d = GlmDistribution::Binomial;
    else if (s == "poisson")  d = GlmDistribution::Poisson;
    else
        throw Error("lassoglm: unsupported distribution '" + s
                    + "' (v1: normal, binomial, poisson)",
                    0, 0, "lassoglm", "", "nk:lassoglm:badDistr");
    double alpha = 1.0;
    if (args.size() >= 5 && !args[4].isEmpty())
        alpha = args[4].toScalar();
    auto r = lassoglm(args[0], args[1], d, args[3], alpha, ctx.engine->resource());
    outs[0] = std::move(r.B);
    if (nargout > 1) outs[1] = std::move(r.Intercept);
    if (nargout > 2) outs[2] = std::move(r.Lambda);
}

} // namespace detail

} // namespace nk::stats
