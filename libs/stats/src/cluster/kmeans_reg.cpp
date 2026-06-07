// libs/signal/src/cluster/kmeans_reg.cpp
//
// CallContext register half of cluster/kmeans.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/stats/cluster/kmeans.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "kmeans_detail.hpp"
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

void kmeans_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kmeans: requires (X, K[, N-V pairs])",
                    0, 0, "kmeans", "", "numkit:kmeans:nargin");
    const int K = (int)args[1].toScalar();
    int max_iter  = 100;
    int replicates = 1;
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
        else if (key == "distance") {
            const std::string dn = lower(v.toString());
            if (dn != "sqeuclidean" && dn != "squaredeuclidean")
                throw Error("kmeans: only Distance = 'sqeuclidean' is "
                            "implemented (got '" + dn + "')",
                            0, 0, "kmeans", "", "numkit:kmeans:distance");
        }
        else if (key == "start") {
            const std::string sn = lower(v.toString());
            if (sn != "plus")
                throw Error("kmeans: only Start = 'plus' is implemented "
                            "(got '" + sn + "')",
                            0, 0, "kmeans", "", "numkit:kmeans:start");
        }
        // 'Display' / 'EmptyAction' / 'OnlinePhase' / 'Options' silently
        // accepted (no-op).
    }
    auto R = kmeans_full(args[0], K, max_iter, replicates, ctx.engine->resource());
    outs[0] = std::move(R.idx);
    if (nargout > 1) outs[1] = std::move(R.C);
    if (nargout > 2) outs[2] = std::move(R.sumd);
    if (nargout > 3) outs[3] = std::move(R.D);
}

} // namespace detail

} // namespace numkit::stats
