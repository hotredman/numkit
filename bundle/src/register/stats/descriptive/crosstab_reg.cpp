// toolboxes/signal/src/descriptive/crosstab_reg.cpp
//
// CallContext register half of descriptive/crosstab.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "descriptive/crosstab_detail.hpp"
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

void crosstab_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("crosstab: requires (x [, y])",
                    0, 0, "crosstab", "", "numkit:crosstab:nargin");
    auto *mr = ctx.engine->resource();
    const Value &y_opt = (args.size() >= 2) ? args[1] : Value::Empty;
    auto [T, chi2, p] = crosstab(args[0], y_opt, mr);
    outs[0] = std::move(T);
    if (nargout > 1) outs[1] = Value::scalar(chi2, mr);
    if (nargout > 2) outs[2] = Value::scalar(p, mr);
    if (nargout > 3) {
        // labels: (maxCategories × numVars) cell. Column j = num2str of
        // each sorted-unique value of variable j (char row); rows beyond
        // a variable's category count are padded with [] (empty double),
        // matching MATLAB R2025b.
        std::vector<std::vector<double>> uniques;
        uniques.push_back(sortedUniqueNoNaN(args[0]));
        if (!y_opt.isEmpty())
            uniques.push_back(sortedUniqueNoNaN(y_opt));
        size_t maxR = 0;
        for (const auto &u : uniques) maxR = std::max(maxR, u.size());
        const size_t numVars = uniques.size();
        Value labels = Value::cell(maxR, numVars, mr);
        for (size_t j = 0; j < numVars; ++j) {
            const auto &u = uniques[j];
            for (size_t i = 0; i < maxR; ++i) {
                Value &cell = labels.cellAt(i + j * maxR);
                cell = (i < u.size())
                           ? Value::fromString(labelString(u[i]), mr)
                           : Value::Empty;
            }
        }
        outs[3] = std::move(labels);
    }
}

} // namespace detail

} // namespace numkit::stats
