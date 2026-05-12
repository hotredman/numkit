// libs/stats/src/cluster/knnsearch.cpp
//
// knnsearch / rangesearch — brute-force NN search via stats::pdist2.

#include <numkit/stats/cluster/knnsearch.hpp>
#include <numkit/stats/cluster/distance.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace numkit::stats {

namespace {

// Parse positional/name-value args. Forms accepted:
//   knnsearch(X, Y)                     → K=1
//   knnsearch(X, Y, K)                  → K-NN, default metric
//   knnsearch(X, Y, 'K', K, ...)        → name-value
//   knnsearch(X, Y, 'Distance', metric, ...)
struct KnnOpts { int K = 1; std::string metric = "euclidean"; double p = 2.0; };

KnnOpts parse_knn_args(Span<const Value> args, const char *fn)
{
    KnnOpts o;
    if (args.size() == 3 && args[2].isNumeric() && args[2].isScalar()) {
        // legacy positional K
        o.K = static_cast<int>(args[2].toScalar());
        return o;
    }
    // name-value pairs
    size_t i = 2;
    while (i < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error(std::string(fn) + ": expected name-value pair",
                        0, 0, fn, "", "m:knn:nvpair");
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (i + 1 >= args.size())
            throw Error(std::string(fn) + ": '" + name + "' missing value",
                        0, 0, fn, "", "m:knn:nvval");
        const Value &v = args[i + 1];
        if (name == "k") {
            o.K = static_cast<int>(v.toScalar());
        } else if (name == "distance") {
            if (!v.isChar() && !v.isString())
                throw Error(std::string(fn) + ": Distance must be a string",
                            0, 0, fn, "", "m:knn:dist");
            o.metric = v.toString();
        } else if (name == "p" || name == "minkowskiexponent") {
            o.p = v.toScalar();
        } else {
            // ignore unknown names (NSMethod, BucketSize, IncludeTies, …)
        }
        i += 2;
    }
    return o;
}

} // anonymous

std::tuple<Value, Value>
knnsearch(const Value &X, const Value &Y, int K, const std::string &metric, double p, std::pmr::memory_resource *mr)
{
    const auto &dx = X.dims();
    const auto &dy = Y.dims();
    const size_t Nx = dx.rows();
    const size_t Ny = dy.rows();
    if (dx.cols() != dy.cols())
        throw Error("knnsearch: X and Y must have the same column count",
                    0, 0, "knnsearch", "", "m:knnsearch:cols");
    if (K < 1)
        throw Error("knnsearch: K must be a positive integer",
                    0, 0, "knnsearch", "", "m:knnsearch:K");
    const int Keff = std::min(static_cast<int>(Nx), K);

    Value Idx = Value::matrix(Ny, static_cast<size_t>(K), ValueType::DOUBLE, mr);
    Value Dout = Value::matrix(Ny, static_cast<size_t>(K), ValueType::DOUBLE, mr);
    double *id = Idx.doubleDataMut();
    double *dd_out = Dout.doubleDataMut();
    if (Nx == 0 || Ny == 0) return {std::move(Idx), std::move(Dout)};

    // pdist2 returns Nx × Ny distance matrix (rows = X, cols = Y) per
    // its doc note "rows of X, rows of Y", but check by fingerprinting:
    // pdist2(X, Y) → size(X,1) × size(Y,1).
    Value D = pdist2(X, Y, metric, p, mr);
    const size_t Dr = D.dims().rows();
    const size_t Dc = D.dims().cols();
    if (Dr != Nx || Dc != Ny)
        throw Error("knnsearch: pdist2 returned unexpected shape",
                    0, 0, "knnsearch", "", "m:knnsearch:internal");
    const double *dd = D.doubleData();

    std::vector<std::pair<double, size_t>> heap;
    heap.reserve(Nx);
    for (size_t q = 0; q < Ny; ++q) {
        // Collect (dist, index) for query q.
        heap.clear();
        for (size_t i = 0; i < Nx; ++i)
            heap.emplace_back(dd[i + q * Nx], i);
        // Partial sort by distance, ties by ascending index (stable).
        std::partial_sort(heap.begin(),
                          heap.begin() + Keff,
                          heap.end(),
                          [](const auto &a, const auto &b) {
                              if (a.first != b.first) return a.first < b.first;
                              return a.second < b.second;
                          });
        for (int k = 0; k < Keff; ++k) {
            id[q + static_cast<size_t>(k) * Ny] =
                static_cast<double>(heap[k].second + 1);  // 1-based
            dd_out[q + static_cast<size_t>(k) * Ny] = heap[k].first;
        }
        // K > Nx: pad rest with NaN.
        for (int k = Keff; k < K; ++k) {
            id[q + static_cast<size_t>(k) * Ny] =
                std::numeric_limits<double>::quiet_NaN();
            dd_out[q + static_cast<size_t>(k) * Ny] =
                std::numeric_limits<double>::quiet_NaN();
        }
    }
    return {std::move(Idx), std::move(Dout)};
}

std::tuple<Value, Value>
rangesearch(const Value &X, const Value &Y, double r, const std::string &metric, double p, std::pmr::memory_resource *mr)
{
    const auto &dx = X.dims();
    const auto &dy = Y.dims();
    const size_t Nx = dx.rows();
    const size_t Ny = dy.rows();
    if (dx.cols() != dy.cols())
        throw Error("rangesearch: X and Y must have the same column count",
                    0, 0, "rangesearch", "", "m:rangesearch:cols");
    if (!(r >= 0.0))
        throw Error("rangesearch: r must be non-negative",
                    0, 0, "rangesearch", "", "m:rangesearch:r");

    Value IdxCells = Value::cell(Ny, 1, mr);
    Value DCells   = Value::cell(Ny, 1, mr);
    if (Nx == 0 || Ny == 0) return {std::move(IdxCells), std::move(DCells)};

    Value D = pdist2(X, Y, metric, p, mr);
    const double *dd = D.doubleData();

    std::vector<std::pair<double, size_t>> hits;
    for (size_t q = 0; q < Ny; ++q) {
        hits.clear();
        for (size_t i = 0; i < Nx; ++i) {
            const double v = dd[i + q * Nx];
            if (v <= r) hits.emplace_back(v, i);
        }
        std::sort(hits.begin(), hits.end(),
                  [](const auto &a, const auto &b) {
                      if (a.first != b.first) return a.first < b.first;
                      return a.second < b.second;
                  });
        const size_t M = hits.size();
        Value rowIdx = Value::matrix(1, M, ValueType::DOUBLE, mr);
        Value rowD   = Value::matrix(1, M, ValueType::DOUBLE, mr);
        if (M > 0) {
            double *ri = rowIdx.doubleDataMut();
            double *rd = rowD.doubleDataMut();
            for (size_t k = 0; k < M; ++k) {
                ri[k] = static_cast<double>(hits[k].second + 1);
                rd[k] = hits[k].first;
            }
        }
        IdxCells.cellAt(q) = std::move(rowIdx);
        DCells.cellAt(q)   = std::move(rowD);
    }
    return {std::move(IdxCells), std::move(DCells)};
}

namespace detail {

void knnsearch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("knnsearch: requires (X, Y [, K | name-value pairs])",
                    0, 0, "knnsearch", "", "m:knnsearch:nargin");
    KnnOpts o = parse_knn_args(args, "knnsearch");
    auto [Idx, D] = knnsearch(args[0], args[1], o.K, o.metric, o.p, ctx.engine->resource());
    outs[0] = std::move(Idx);
    if (nargout > 1) outs[1] = std::move(D);
}

void rangesearch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("rangesearch: requires (X, Y, r [, name-value pairs])",
                    0, 0, "rangesearch", "", "m:rangesearch:nargin");
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
                            0, 0, "rangesearch", "", "m:rangesearch:nvpair");
            std::string name = args[i].toString();
            for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (i + 1 >= args.size())
                throw Error("rangesearch: '" + name + "' missing value",
                            0, 0, "rangesearch", "", "m:rangesearch:nvval");
            const Value &v = args[i + 1];
            if (name == "distance") {
                if (!v.isChar() && !v.isString())
                    throw Error("rangesearch: Distance must be a string",
                                0, 0, "rangesearch", "", "m:rangesearch:dist");
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
