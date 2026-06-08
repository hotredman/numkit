// toolboxes/signal/src/cluster/linkage_reg.cpp
//
// CallContext register half of cluster/linkage.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/cluster/distance.hpp>
#include <numkit/stats/cluster/linkage.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "linkage_detail.hpp"
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

void linkage_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("linkage: requires Y[, method[, metric]]",
                    0, 0, "linkage", "", "numkit:linkage:nargin");
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    std::string method = "single";
    std::string metric = "euclidean";
    double      p      = 2.0;
    if (args.size() >= 2 && (args[1].isChar() || args[1].isString()))
        method = lower(args[1].toString());
    // 3rd arg: metric for the implicit pdist call when Y is raw N×D data.
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        metric = lower(args[2].toString());
    // 4th arg (Minkowski exponent): MATLAB doesn't pass p positionally
    // here, but accept it as a fallback for the parity harness.
    if (args.size() >= 4 && args[3].numel() == 1
        && !(args[3].isChar() || args[3].isString()))
        p = args[3].toScalar();
    outs[0] = linkage(args[0], method, metric, p, ctx.engine->resource());
}

void cluster_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cluster: requires (Z, options)", 0, 0, "cluster", "",
                    "numkit:cluster:nargin");
    int maxclust = -1;
    double cutoff = -1.0;
    int depth = 2;
    // MATLAB default 'cutoff' criterion is 'inconsistent'.
    std::string criterion = "inconsistent";
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    for (size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto s = lower(args[i].toString());
            if      (s == "maxclust")  maxclust  = (int)args[i + 1].toScalar();
            else if (s == "cutoff")    cutoff    = args[i + 1].toScalar();
            else if (s == "criterion") criterion = lower(args[i + 1].toString());
            else if (s == "depth")     depth     = (int)args[i + 1].toScalar();
        }
    }
    outs[0] = cluster_from_linkage(args[0], maxclust, cutoff, criterion, depth, ctx.engine->resource());
}

void clusterdata_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("clusterdata: requires (X, ...)", 0, 0, "clusterdata", "",
                    "numkit:clusterdata:nargin");
    int maxclust = -1;
    double cutoff = -1.0;
    int depth = 2;
    double p = 2.0;
    std::string method     = "single";
    std::string criterion  = "inconsistent";
    std::string distance_metric = "euclidean";
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    // MATLAB scalar shortcut: clusterdata(X, c) where c is numeric.
    //   c >= 2  → maxclust (rounded toward zero)
    //   0 < c < 2 → cutoff (inconsistency criterion)
    // Verified via R2025b probe.
    if (args.size() >= 2 && args[1].numel() == 1
        && !(args[1].isChar() || args[1].isString())) {
        const double c = args[1].toScalar();
        if (c >= 2.0) maxclust = (int)c;
        else if (c > 0.0) cutoff = c;
    }
    for (size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto s = lower(args[i].toString());
            if      (s == "maxclust")  maxclust  = (int)args[i + 1].toScalar();
            else if (s == "cutoff")    cutoff    = args[i + 1].toScalar();
            else if (s == "linkage")   method    = lower(args[i + 1].toString());
            else if (s == "criterion") criterion = lower(args[i + 1].toString());
            else if (s == "depth")     depth     = (int)args[i + 1].toScalar();
            else if (s == "distance")  distance_metric = lower(args[i + 1].toString());
            else if (s == "p")         p         = args[i + 1].toScalar();
            // 'savememory' and other doc'd N-V silently ignored.
        }
    }
    outs[0] = clusterdata(args[0], maxclust, cutoff, method, criterion, depth, distance_metric, p, ctx.engine->resource());
}

void cophenet_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cophenet: requires (Z, Y)", 0, 0, "cophenet", "",
                    "numkit:cophenet:nargin");
    auto [c, d] = cophenet_full(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(c);
    if (nargout > 1) outs[1] = std::move(d);
}

void inconsistent_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("inconsistent: requires (Z[, depth])", 0, 0, "inconsistent",
                    "", "numkit:inconsistent:nargin");
    int depth = (args.size() >= 2 && !args[1].isEmpty())
                ? (int)args[1].toScalar() : 2;
    outs[0] = inconsistent(args[0], depth, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
