// toolboxes/signal/src/regress/glm_reg.cpp
//
// CallContext register half of regress/glm.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/distributions/normal.hpp>     // for probit via norminv/normcdf
#include <numkit/stats/regress/regress.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "glm_detail.hpp"
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

static GlmDistribution parseDistr(const std::string &s, const char *fn)
{
    if (s == "normal")             return GlmDistribution::Normal;
    if (s == "binomial")           return GlmDistribution::Binomial;
    if (s == "poisson")            return GlmDistribution::Poisson;
    if (s == "gamma")              return GlmDistribution::Gamma;
    if (s == "inverse gaussian"
        || s == "inversegaussian") return GlmDistribution::InverseGaussian;
    throw Error(std::string(fn) + ": unknown distribution '" + s + "'",
                0, 0, fn, "", std::string("numkit:") + fn + ":badDistr");
}

static GlmLink parseLink(const std::string &s, const char *fn)
{
    if (s.empty() || s == "canonical") return GlmLink::Identity;
    if (s == "identity")    return GlmLink::Identity;
    if (s == "logit")       return GlmLink::Logit;
    if (s == "log")         return GlmLink::Log;
    if (s == "reciprocal")  return GlmLink::Reciprocal;
    if (s == "probit")      return GlmLink::Probit;
    throw Error(std::string(fn) + ": unknown link '" + s + "'",
                0, 0, fn, "", std::string("numkit:") + fn + ":badLink");
}

void glmfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("glmfit: requires (X, y, distr [, link])",
                    0, 0, "glmfit", "", "numkit:glmfit:nargin");
    if (!args[2].isChar())
        throw Error("glmfit: distr must be a string",
                    0, 0, "glmfit", "", "numkit:glmfit:badDistr");
    const GlmDistribution d = parseDistr(args[2].toString(), "glmfit");
    GlmLink link = GlmLink::Identity;
    if (args.size() >= 4 && args[3].isChar())
        link = parseLink(args[3].toString(), "glmfit");
    auto r = glmfit(args[0], args[1], d, link, ctx.engine->resource());
    outs[0] = std::move(r.b);
    if (nargout > 1) outs[1] = std::move(r.dev);
}

void glmval_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("glmval: requires (b, X, link)",
                    0, 0, "glmval", "", "numkit:glmval:nargin");
    if (!args[2].isChar())
        throw Error("glmval: link must be a string",
                    0, 0, "glmval", "", "numkit:glmval:badLink");
    GlmLink link = parseLink(args[2].toString(), "glmval");
    outs[0] = glmval(args[0], args[1], link, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
