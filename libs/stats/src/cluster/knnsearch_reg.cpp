// libs/signal/src/cluster/knnsearch_reg.cpp
//
// CallContext register half of cluster/knnsearch.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/cluster/distance.hpp>
#include <numkit/stats/cluster/knnsearch.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "knnsearch_detail.hpp"
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

void knnsearch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("knnsearch: requires (X, Y [, K | name-value pairs])",
                    0, 0, "knnsearch", "", "numkit:knnsearch:nargin");
    KnnOpts o = parse_knn_args(args, "knnsearch");
    auto [Idx, D] = knnsearch(args[0], args[1], o.K, o.metric, o.p, ctx.engine->resource());
    outs[0] = std::move(Idx);
    if (nargout > 1) outs[1] = std::move(D);
}

void rangesearch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("rangesearch: requires (X, Y, r [, name-value pairs])",
                    0, 0, "rangesearch", "", "numkit:rangesearch:nargin");
    const double r = args[2].toScalar();
    KnnOpts o;
    // parse remaining name-value
    if (args.size() > 3) {
        // shift args >= 3 into a temp slice for parser by reusing logic
        // (pdist2 args 0/1 are the X,Y placeholders for parse_knn_args).
        // Simpler: hand-parse here.
        size_t i = 3;
        while (i < args.size()) {
            if (!args[i].isChar() && !args[i].isString())
                throw Error("rangesearch: expected name-value pair",
                            0, 0, "rangesearch", "", "numkit:rangesearch:nvpair");
            std::string name = args[i].toString();
            for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (i + 1 >= args.size())
                throw Error("rangesearch: '" + name + "' missing value",
                            0, 0, "rangesearch", "", "numkit:rangesearch:nvval");
            const Value &v = args[i + 1];
            if (name == "distance") {
                if (!v.isChar() && !v.isString())
                    throw Error("rangesearch: Distance must be a string",
                                0, 0, "rangesearch", "", "numkit:rangesearch:dist");
                o.metric = v.toString();
            } else if (name == "p" || name == "minkowskiexponent") {
                o.p = v.toScalar();
            }
            i += 2;
        }
    }
    auto [Idx, D] = rangesearch(args[0], args[1], r, o.metric, o.p, ctx.engine->resource());
    outs[0] = std::move(Idx);
    if (nargout > 1) outs[1] = std::move(D);
}

} // namespace detail

} // namespace numkit::stats
