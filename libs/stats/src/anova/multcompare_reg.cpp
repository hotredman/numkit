// libs/signal/src/anova/multcompare_reg.cpp
//
// CallContext register half of anova/multcompare.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/anova/anova.hpp>
#include <numkit/stats/distributions/students_t.hpp>   // tinv, tcdf
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
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

void multcompare_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("multcompare: requires (stats [, alpha [, ctype]])",
                    0, 0, "multcompare", "", "numkit:multcompare:nargin");
    double alpha = 0.05;
    if (args.size() >= 2 && !args[1].isEmpty())
        alpha = args[1].toScalar();
    McCorrection ctype = McCorrection::Bonferroni;
    if (args.size() >= 3 && args[2].isChar()) {
        const std::string s = args[2].toString();
        if (s == "bonferroni") ctype = McCorrection::Bonferroni;
        else if (s == "lsd")   ctype = McCorrection::LSD;
        else if (s == "tukey-kramer" || s == "hsd")
            throw Error("multcompare: 'tukey-kramer' not yet supported "
                        "(v1 ships 'bonferroni' and 'lsd' only)",
                        0, 0, "multcompare", "", "numkit:multcompare:badCtype");
        else
            throw Error("multcompare: unknown ctype '" + s + "'",
                        0, 0, "multcompare", "", "numkit:multcompare:badCtype");
    }
    outs[0] = multcompare(args[0], alpha, ctype, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
