// toolboxes/signal/src/cluster/silhouette_reg.cpp
//
// CallContext register half of cluster/silhouette.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/stats/cluster/distance.hpp>
#include <nk/stats/cluster/silhouette.hpp>
#include <nk/value/error.hpp>
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

void silhouette_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("silhouette: requires (X, clust [, metric [, p]])",
                    0, 0, "silhouette", "", "nk:silhouette:nargin");
    std::string metric = "sqeuclidean";
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("silhouette: metric must be a string",
                        0, 0, "silhouette", "", "nk:silhouette:metric");
        metric = args[2].toString();
    }
    double p = 2.0;
    if (args.size() >= 4 && !args[3].isEmpty()) p = args[3].toScalar();
    outs[0] = silhouette(args[0], args[1], metric, p, ctx.engine->resource());
}

} // namespace detail

} // namespace nk::stats
