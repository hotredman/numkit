// toolboxes/signal/src/anova/multcompare_reg.cpp
//
// CallContext register half of anova/multcompare.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/stats/anova/anova.hpp>
#include <nk/stats/distributions/students_t.hpp>   // tinv, tcdf
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/value.hpp>
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

void multcompare_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("multcompare: requires (stats [, alpha [, ctype]])",
                    0, 0, "multcompare", "", "nk:multcompare:nargin");
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
                        0, 0, "multcompare", "", "nk:multcompare:badCtype");
        else
            throw Error("multcompare: unknown ctype '" + s + "'",
                        0, 0, "multcompare", "", "nk:multcompare:badCtype");
    }
    outs[0] = multcompare(args[0], alpha, ctype, ctx.engine->resource());
}

} // namespace detail

} // namespace nk::stats
