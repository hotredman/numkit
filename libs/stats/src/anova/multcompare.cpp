// libs/stats/src/anova/multcompare.cpp
//
// Multiple-comparison post-hoc tests for one-way ANOVA results.
// v1: Bonferroni-corrected pairwise t-tests + Fisher LSD (raw).
// KNOWN GAP: Tukey-Kramer HSD (MATLAB default) needs the studentized
// range distribution — not yet shipped.

#include <numkit/stats/anova/anova.hpp>

#include <numkit/stats/distributions/students_t.hpp>   // tinv, tcdf

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::stats {

namespace {

// Two-sided t p-value: 2 * (1 - tcdf(|t|, df)).
double tTwoSidedP(double tStat, double df, std::pmr::memory_resource *mr)
{
    Value tV = Value::scalar(std::fabs(tStat), mr);
    const double F = tcdf(tV, df, mr).toScalar();
    return std::min(1.0, std::max(0.0, 2.0 * (1.0 - F)));
}

} // namespace

Value multcompare(const Value &stats, double alpha, McCorrection ctype,
                   std::pmr::memory_resource *mr)
{
    if (!stats.isStruct())
        throw Error("multcompare: stats must be a struct (from anova1)",
                    0, 0, "multcompare", "", "numkit:multcompare:notStruct");
    if (!stats.hasField("means") || !stats.hasField("n")
        || !stats.hasField("s") || !stats.hasField("df"))
        throw Error("multcompare: stats must have fields {means, n, s, df}",
                    0, 0, "multcompare", "", "numkit:multcompare:missingField");
    if (alpha <= 0.0 || alpha >= 1.0)
        throw Error("multcompare: alpha must be in (0, 1)",
                    0, 0, "multcompare", "", "numkit:multcompare:badAlpha");

    const Value &means = stats.field("means");
    const Value &nv    = stats.field("n");
    const double s     = stats.field("s").toScalar();
    const double df    = stats.field("df").toScalar();
    const std::size_t K = means.numel();
    if (K < 2)
        throw Error("multcompare: need at least 2 groups",
                    0, 0, "multcompare", "", "numkit:multcompare:tooFewGroups");

    std::vector<double> m(K), n(K);
    for (std::size_t i = 0; i < K; ++i) {
        m[i] = means.elemAsDouble(i);
        n[i] = nv.elemAsDouble(i);
    }

    // Number of pairs K · (K - 1) / 2.
    const std::size_t nPairs = K * (K - 1) / 2;

    // Bonferroni: critical level for the simultaneous CI is alpha / K_pairs.
    double alphaEffective;
    switch (ctype) {
    case McCorrection::Bonferroni:
        alphaEffective = alpha / static_cast<double>(nPairs);
        break;
    case McCorrection::LSD:
    default:
        alphaEffective = alpha;
        break;
    }
    Value pTail = Value::scalar(1.0 - alphaEffective / 2.0, mr);
    const double tCrit = tinv(pTail, df, mr).toScalar();

    auto out = Value::matrix(nPairs, 6, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    std::size_t row = 0;
    for (std::size_t i = 0; i < K; ++i) {
        for (std::size_t j = i + 1; j < K; ++j) {
            const double diff = m[i] - m[j];
            const double se   = s * std::sqrt(1.0 / n[i] + 1.0 / n[j]);
            const double halfCI = tCrit * se;
            const double tStat  = (se > 0.0) ? diff / se : 0.0;
            double pVal = tTwoSidedP(tStat, df, mr);
            if (ctype == McCorrection::Bonferroni)
                pVal = std::min(1.0, pVal * static_cast<double>(nPairs));

            // Column-major: out(row, c) = od[c * nPairs + row].
            od[0 * nPairs + row] = static_cast<double>(i + 1);
            od[1 * nPairs + row] = static_cast<double>(j + 1);
            od[2 * nPairs + row] = diff - halfCI;
            od[3 * nPairs + row] = diff;
            od[4 * nPairs + row] = diff + halfCI;
            od[5 * nPairs + row] = pVal;
            ++row;
        }
    }
    return out;
}

// ── Adapter ─────────────────────────────────────────────────────────
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
