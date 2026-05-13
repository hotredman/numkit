// libs/stats/src/cluster/silhouette.cpp
//
// silhouette(X, clust [, metric, p]) — clustering quality metric.
// Built on top of stats::pdist2 for the distance matrix; the rest
// is a per-point average + per-cluster aggregation.

#include <numkit/stats/cluster/silhouette.hpp>
#include <numkit/stats/cluster/distance.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace numkit::stats {

Value silhouette(const Value &X, const Value &clust, const std::string &metric, double p, std::pmr::memory_resource *mr)
{
    const auto &dx = X.dims();
    const size_t N = dx.rows();
    if (N == 0)
        return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    if (clust.numel() != N)
        throw Error("silhouette: clust length must equal size(X, 1)",
                    0, 0, "silhouette", "", "m:silhouette:size");

    // Pull labels into int vector. MATLAB allows numeric or char/string —
    // we accept anything that elemAsDouble can read (round to int).
    std::vector<int> lbl(N);
    for (size_t i = 0; i < N; ++i)
        lbl[i] = static_cast<int>(std::lround(clust.elemAsDouble(i)));

    // Catalogue labels and their member indices.
    std::unordered_map<int, std::vector<size_t>> groups;
    groups.reserve(N);
    for (size_t i = 0; i < N; ++i) groups[lbl[i]].push_back(i);

    // N×N distance matrix from pdist2.
    Value D = pdist2(X, X, metric, p, mr);
    if (D.dims().rows() != N || D.dims().cols() != N)
        throw Error("silhouette: pdist2 returned unexpected shape",
                    0, 0, "silhouette", "", "m:silhouette:internal");
    const double *dd = D.doubleData();

    Value out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    for (size_t i = 0; i < N; ++i) {
        const int k = lbl[i];
        const auto &mine = groups[k];

        // a(i): mean distance to other points in same cluster.
        double a = 0.0;
        if (mine.size() <= 1) {
            // Singleton: s(i) = 0 by MATLAB convention.
            od[i] = 0.0;
            continue;
        }
        for (size_t j : mine) if (j != i)
            a += dd[i + j * N];   // column-major: D(i, j) = dd[i + j*N]
        a /= static_cast<double>(mine.size() - 1);

        // b(i): min over other clusters of mean distance to that cluster.
        double b = std::numeric_limits<double>::infinity();
        for (const auto &kv : groups) {
            if (kv.first == k) continue;
            double m = 0.0;
            for (size_t j : kv.second) m += dd[i + j * N];
            m /= static_cast<double>(kv.second.size());
            if (m < b) b = m;
        }
        if (!std::isfinite(b)) {
            // Only one cluster in total — undefined; MATLAB returns NaN
            // (per behaviour with one group). We mirror that.
            od[i] = std::numeric_limits<double>::quiet_NaN();
            continue;
        }

        const double denom = std::max(a, b);
        od[i] = (denom > 0.0) ? (b - a) / denom : 0.0;
    }
    return out;
}

namespace detail {

void silhouette_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("silhouette: requires (X, clust [, metric [, p]])",
                    0, 0, "silhouette", "", "m:silhouette:nargin");
    std::string metric = "sqeuclidean";
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("silhouette: metric must be a string",
                        0, 0, "silhouette", "", "m:silhouette:metric");
        metric = args[2].toString();
    }
    double p = 2.0;
    if (args.size() >= 4 && !args[3].isEmpty()) p = args[3].toScalar();
    outs[0] = silhouette(args[0], args[1], metric, p, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
