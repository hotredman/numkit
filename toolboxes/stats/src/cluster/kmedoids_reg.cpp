// toolboxes/signal/src/cluster/kmedoids_reg.cpp
//
// CallContext register half of cluster/kmedoids.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/stats/cluster/kmedoids.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "kmedoids_detail.hpp"
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

void kmedoids_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kmedoids: requires (X, K[, N-V pairs])",
                    0, 0, "kmedoids", "", "numkit:kmedoids:nargin");
    const int K = (int)args[1].toScalar();
    int max_iter  = 100;
    int replicates = 1;
    // MATLAB R2025b kmedoids default 'Distance' is 'sqeuclidean'.
    std::string metric    = "sqeuclidean";
    std::string algorithm = "pam";
    std::string start     = "plus";
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        const std::string key = lower(args[i].toString());
        const Value &v = args[i + 1];
        if      (key == "maxiter")    max_iter   = (int)v.toScalar();
        else if (key == "replicates") replicates = (int)v.toScalar();
        else if (key == "distance")   metric     = lower(v.toString());
        else if (key == "algorithm")  algorithm  = lower(v.toString());
        else if (key == "start")      start      = lower(v.toString());
        // 'OnlinePhase' / 'Options' / 'PercentNeighbors' silently accepted.
    }
    auto R = kmedoids_full(args[0], K, max_iter, replicates, metric, ctx.engine->resource());
    outs[0] = std::move(R.idx);
    if (nargout > 1) outs[1] = std::move(R.C);
    if (nargout > 2) outs[2] = std::move(R.sumd);
    if (nargout > 3) outs[3] = std::move(R.D);
    if (nargout > 4) outs[4] = std::move(R.midx);
    if (nargout > 5) {
        std::pmr::memory_resource *mr = ctx.engine->resource();
        Value info = Value::structure(mr);
        info.field("algorithm")     = Value::fromString(algorithm, mr);
        info.field("start")         = Value::fromString(start, mr);
        info.field("distance")      = Value::fromString(metric, mr);
        info.field("iterations")    = Value::scalar(double(R.iters), mr);
        info.field("bestReplicate") = Value::scalar(double(R.best_rep), mr);
        outs[5] = std::move(info);
    }
}

void dbscan_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("dbscan: requires (X, eps, minpts[, N-V pairs])",
                    0, 0, "dbscan", "", "numkit:dbscan:nargin");
    const double eps    = args[1].toScalar();
    const int    minpts = (int)args[2].toScalar();
    std::string metric  = "euclidean";
    double p = 2.0;
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    // MATLAB requires N-V form; we also tolerate a 4th positional metric
    // string for backward compat with prior numkit usage.
    size_t i = 3;
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string s = lower(args[i].toString());
        if (s != "distance" && s != "p" && s != "cov" && s != "scale") {
            metric = s;
            ++i;
        }
    }
    for (; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        const std::string key = lower(args[i].toString());
        const Value &val = args[i + 1];
        if      (key == "distance") metric = lower(val.toString());
        else if (key == "p")        p      = val.toScalar();
        // 'Cov' and 'Scale' silently accepted but ignored — only matter
        // for mahalanobis / seuclidean which dbscan() doesn't yet wire up.
    }
    auto [idx, core] = dbscan(args[0], eps, minpts, metric, p, ctx.engine->resource());
    outs[0] = std::move(idx);
    if (nargout > 1) outs[1] = std::move(core);
}

} // namespace detail

} // namespace numkit::stats
