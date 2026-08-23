// toolboxes/signal/src/sampling/lhs_reg.cpp
//
// CallContext register half of sampling/lhs.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/datafun.hpp>
#include <numkit/stats/distributions/normal.hpp>
#include <numkit/stats/sampling/lhs.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
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

void lhsdesign_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lhsdesign: requires (n, p[, Name, Value, ...])",
                    0, 0, "lhsdesign", "", "numkit:lhsdesign:nargin");
    const std::size_t n = static_cast<std::size_t>(args[0].toScalar());
    const std::size_t p = static_cast<std::size_t>(args[1].toScalar());
    bool smooth = true;
    LhsCriterion crit = LhsCriterion::Maximin;
    std::size_t iters = 5;
    // Parse name-value pairs from args[2..].
    for (std::size_t k = 2; k + 1 < args.size(); k += 2) {
        if (!args[k].isChar() && !args[k].isString())
            throw Error("lhsdesign: name-value arguments expected",
                        0, 0, "lhsdesign", "", "numkit:lhsdesign:badNameValue");
        std::string name = args[k].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "smooth") {
            std::string v = args[k + 1].toString();
            for (auto &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            smooth = (v != "off");
        } else if (name == "criterion") {
            std::string v = args[k + 1].toString();
            for (auto &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if      (v == "none")        crit = LhsCriterion::None;
            else if (v == "maximin")     crit = LhsCriterion::Maximin;
            else if (v == "correlation") crit = LhsCriterion::Correlation;
            else throw Error("lhsdesign: unknown criterion '" + v + "'",
                             0, 0, "lhsdesign", "", "numkit:lhsdesign:badCriterion");
        } else if (name == "iterations") {
            iters = static_cast<std::size_t>(args[k + 1].toScalar());
            if (iters < 1) iters = 1;
        } else {
            throw Error("lhsdesign: unknown option '" + name + "'",
                        0, 0, "lhsdesign", "", "numkit:lhsdesign:badOption");
        }
    }
    outs[0] = lhsdesign(ctx.engine->rng(), n, p, smooth, crit, iters, ctx.engine->resource());
}

void lhsnorm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("lhsnorm: requires (mu, Sigma, n)",
                    0, 0, "lhsnorm", "", "numkit:lhsnorm:nargin");
    const std::size_t n = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = lhsnorm(ctx.engine->rng(), args[0], args[1], n, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
