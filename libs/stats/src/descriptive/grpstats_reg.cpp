// libs/signal/src/descriptive/grpstats_reg.cpp
//
// CallContext register half of descriptive/grpstats.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/descriptive/descriptive.hpp>
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

void grpstats_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("grpstats: requires (X, group [, fn])",
                    0, 0, "grpstats", "", "numkit:grpstats:nargin");
    auto *mr = ctx.engine->resource();

    std::vector<std::string> fn_names;
    if (args.size() >= 3) {
        const Value &fn_arg = args[2];
        if (fn_arg.isChar() || fn_arg.isString()) {
            fn_names.push_back(fn_arg.toString());
        } else if (fn_arg.isCell()) {
            for (size_t i = 0; i < fn_arg.numel(); ++i) {
                const Value &el = fn_arg.cellAt(i);
                if (!(el.isChar() || el.isString()))
                    throw Error("grpstats: cell fn names must be strings",
                                0, 0, "grpstats", "",
                                "numkit:grpstats:BadCell");
                fn_names.push_back(el.toString());
            }
        } else {
            throw Error("grpstats: fn arg must be a string or cell of strings",
                        0, 0, "grpstats", "", "numkit:grpstats:BadFnArg");
        }
    }

    // With no whichstats given, MATLAB's default outputs are
    // [means, sem, counts] — emit as many as the caller requested.
    if (fn_names.empty()) {
        static const char *kDefault[] = {"mean", "sem", "numel"};
        const size_t k = std::min<size_t>(std::max<size_t>(nargout, 1), 3);
        for (size_t i = 0; i < k; ++i) fn_names.emplace_back(kDefault[i]);
    }

    auto results = grpstats(args[0], args[1], fn_names, mr);
    // Distribute results across outputs (1-output gets the first;
    // multi-fn cell-of-fn fills successive outputs).
    const size_t emit = std::min<size_t>(results.size(),
                                         std::max<size_t>(nargout, 1));
    for (size_t i = 0; i < emit; ++i)
        outs[i] = std::move(results[i]);
}

} // namespace detail

} // namespace numkit::stats
