// libs/signal/src/cluster/distance_reg.cpp
//
// CallContext register half of cluster/distance.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/cluster/distance.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "distance_detail.hpp"
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

namespace {
struct MetricArgs {
    std::string metric;
    double p;
    const Value *C;       // Mahalanobis covariance (nullable).
};
MetricArgs parse_metric_args(Span<const Value> args, size_t start) {
    MetricArgs out{"euclidean", 2.0, nullptr};
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            out.metric = args[i].toString();
        } else if (args[i].numel() == 1) {
            out.p = args[i].toScalar();
        } else if (args[i].numel() > 1) {
            // Multi-element arg: treated as Mahalanobis covariance C.
            out.C = &args[i];
        }
    }
    return out;
}
} // anonymous

void pdist_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pdist: requires X[, metric[, p|C]]", 0, 0, "pdist", "",
                    "numkit:pdist:nargin");
    auto a = parse_metric_args(args, 1);
    outs[0] = pdist(args[0], a.metric, a.p, a.C, ctx.engine->resource());
}

void pdist2_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pdist2: requires (X, Y[, metric[, p|C]])", 0, 0, "pdist2", "",
                    "numkit:pdist2:nargin");

    // Walk args[2..]: extract optional 'Smallest'/'Largest' N-V pair, then
    // pass the rest to the standard metric/p/C parser.
    ScratchArena scratch(ctx.engine->resource());
    std::pmr::vector<Value> filtered(&scratch);
    filtered.reserve(args.size());
    bool topk_mode = false;
    bool largest = false;
    size_t k = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        if ((args[i].isChar() || args[i].isString()) && i + 1 < args.size()) {
            const std::string s = args[i].toString();
            std::string sl; sl.reserve(s.size());
            for (char c : s) sl.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            if (sl == "smallest" || sl == "largest") {
                topk_mode = true;
                largest = (sl == "largest");
                k = static_cast<size_t>(args[i + 1].toScalar());
                ++i; // skip value
                continue;
            }
        }
        filtered.push_back(args[i]);
    }

    // Build a mini-args view for parse_metric_args (using filtered).
    // parse_metric_args expects a Span starting at offset; we re-use by
    // creating a small temporary Vector of Values prepended with two dummies
    // (skipped via start=2). Simpler: inline parse here on the filtered vec.
    MetricArgs a{"euclidean", 2.0, nullptr};
    for (size_t i = 0; i < filtered.size(); ++i) {
        if (filtered[i].isChar() || filtered[i].isString()) {
            a.metric = filtered[i].toString();
        } else if (filtered[i].numel() == 1) {
            a.p = filtered[i].toScalar();
        } else if (filtered[i].numel() > 1) {
            a.C = &filtered[i];
        }
    }

    if (topk_mode) {
        Value D, I;
        pdist2_topk(args[0], args[1], a.metric, a.p, a.C, k, largest, D, I, ctx.engine->resource());
        outs[0] = D;
        if (outs.size() > 1) outs[1] = I;
    } else {
        outs[0] = pdist2(args[0], args[1], a.metric, a.p, a.C, ctx.engine->resource());
    }
}

void squareform_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("squareform: requires d", 0, 0, "squareform", "",
                    "numkit:squareform:nargin");
    outs[0] = squareform(args[0], ctx.engine->resource());
}

void mahal_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mahal: requires (Y, X)", 0, 0, "mahal", "",
                    "numkit:mahal:nargin");
    outs[0] = mahal(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
