// libs/stats/src/cluster/kmedoids.cpp

#include <numkit/stats/cluster/kmedoids.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <mutex>
#include <random>

namespace numkit::stats {

namespace {

enum class Metric { Euclidean, SqEuclidean, Cityblock, Chebychev,
                    Minkowski, Cosine, Hamming, Jaccard, Precomputed };

Metric parse_metric(const std::string &s) {
    std::string sl; sl.reserve(s.size());
    for (char c : s) sl.push_back((char)std::tolower((unsigned char)c));
    if (sl == "euclidean")        return Metric::Euclidean;
    if (sl == "squaredeuclidean" ||
        sl == "sqeuclidean")      return Metric::SqEuclidean;
    if (sl == "cityblock")        return Metric::Cityblock;
    if (sl == "chebychev")        return Metric::Chebychev;
    if (sl == "minkowski")        return Metric::Minkowski;
    if (sl == "cosine")           return Metric::Cosine;
    if (sl == "hamming")          return Metric::Hamming;
    if (sl == "jaccard")          return Metric::Jaccard;
    if (sl == "precomputed")      return Metric::Precomputed;
    return Metric::Euclidean;
}

inline double dist_pair(const double *a, const double *b, size_t D, Metric m,
                        double p = 2.0) {
    switch (m) {
        case Metric::Euclidean: {
            double s = 0.0;
            for (size_t k = 0; k < D; ++k) { double d = a[k] - b[k]; s += d * d; }
            return std::sqrt(s);
        }
        case Metric::SqEuclidean: {
            double s = 0.0;
            for (size_t k = 0; k < D; ++k) { double d = a[k] - b[k]; s += d * d; }
            return s;
        }
        case Metric::Cityblock: {
            double s = 0.0;
            for (size_t k = 0; k < D; ++k) s += std::fabs(a[k] - b[k]);
            return s;
        }
        case Metric::Chebychev: {
            double m_ = 0.0;
            for (size_t k = 0; k < D; ++k) {
                double d = std::fabs(a[k] - b[k]);
                if (d > m_) m_ = d;
            }
            return m_;
        }
        case Metric::Minkowski: {
            double s = 0.0;
            for (size_t k = 0; k < D; ++k) {
                double d = std::fabs(a[k] - b[k]);
                s += std::pow(d, p);
            }
            return std::pow(s, 1.0 / p);
        }
        case Metric::Cosine: {
            double xy = 0, xx = 0, yy = 0;
            for (size_t k = 0; k < D; ++k) {
                xy += a[k] * b[k];
                xx += a[k] * a[k];
                yy += b[k] * b[k];
            }
            const double denom = std::sqrt(xx) * std::sqrt(yy);
            return (denom > 0.0) ? (1.0 - xy / denom) : 1.0;
        }
        case Metric::Hamming: {
            int diff = 0;
            for (size_t k = 0; k < D; ++k) if (a[k] != b[k]) ++diff;
            return double(diff) / double(D);
        }
        case Metric::Jaccard: {
            int diff = 0, considered = 0;
            for (size_t k = 0; k < D; ++k) {
                const bool xi = a[k] != 0.0, yi = b[k] != 0.0;
                if (xi || yi) {
                    ++considered;
                    if (xi != yi) ++diff;
                }
            }
            return considered > 0 ? double(diff) / double(considered) : 0.0;
        }
        case Metric::Precomputed:
            return 0.0;  // never used through this dispatch
    }
    return 0.0;
}

// Read X (N×D, column-major) into a flat row-major ScratchVec.
ScratchVec<double> read_rows(const Value &X, std::pmr::memory_resource *scratch_mr) {
    const size_t N = X.dims().rows();
    const size_t D = X.dims().cols();
    ScratchVec<double> out(N * D, scratch_mr);
    for (size_t r = 0; r < N; ++r)
        for (size_t c = 0; c < D; ++c)
            out[r * D + c] = X.elemAsDouble(c * N + r);
    return out;
}

} // anonymous

// Full kmedoids result: (idx, C, sumd, D, midx).
// `info` (the 6th MATLAB output) is built by the adapter via Value::structure.
struct KmedoidsResult {
    Value idx;     // N×1
    Value C;       // K×D — coordinates of medoid points
    Value sumd;    // K×1
    Value D;       // N×K distances point-to-medoid
    Value midx;    // K×1 — 1-based row indices of medoids in X
    int   iters;   // best replicate iteration count
    int   best_rep;
};

KmedoidsResult
kmedoids_full(std::pmr::memory_resource *mr, const Value &X, int K,
              int max_iter, int replicates, const std::string &metric_name)
{
    if (max_iter <= 0)   max_iter = 100;
    if (replicates <= 0) replicates = 1;
    const Metric m = parse_metric(metric_name);

    const size_t N = X.dims().rows();
    const size_t D = X.dims().cols();
    if (K < 1 || (size_t)K > N)
        throw Error("kmedoids: K must be in 1..N", 0, 0, "kmedoids", "",
                    "m:kmedoids:badK");

    ScratchArena scratch(mr);
    ScratchVec<double> Xv = read_rows(X, &scratch);
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();

    ScratchVec<int>    best_med((size_t)K, &scratch);
    ScratchVec<int>    best_idx(N, &scratch);
    ScratchVec<double> best_sumd((size_t)K, &scratch);
    double best_total = std::numeric_limits<double>::infinity();
    int    best_iters = 0;
    int    best_rep   = 0;

    ScratchVec<int>    med((size_t)K, &scratch);
    ScratchVec<int>    idx(N, &scratch);
    ScratchVec<double> sumd((size_t)K, &scratch);
    ScratchVec<int>    members(&scratch);
    members.reserve(N);

    for (int rep = 0; rep < replicates; ++rep) {
        // Random initial medoids — sample K distinct rows.
        {
            std::lock_guard<std::mutex> lk(mtx);
            ScratchVec<int> all(N, &scratch);
            for (size_t i = 0; i < N; ++i) all[i] = (int)i;
            std::shuffle(all.begin(), all.end(), gen);
            for (int k = 0; k < K; ++k) med[k] = all[k];
        }

        int rep_iters = 0;
        for (int iter = 0; iter < max_iter; ++iter) {
            ++rep_iters;
            // Assign each point to nearest medoid.
            std::fill(sumd.begin(), sumd.end(), 0.0);
            double total = 0.0;
            for (size_t i = 0; i < N; ++i) {
                int best = 0;
                double bd = std::numeric_limits<double>::infinity();
                for (int k = 0; k < K; ++k) {
                    const double d = dist_pair(&Xv[i * D],
                                                &Xv[(size_t)med[k] * D], D, m);
                    if (d < bd) { bd = d; best = k; }
                }
                idx[i] = best;
                sumd[best] += bd;
                total += bd;
            }

            // Update each medoid: among its members, pick the one whose
            // sum of distances to the rest is minimal.
            bool changed = false;
            for (int k = 0; k < K; ++k) {
                members.clear();
                for (size_t i = 0; i < N; ++i)
                    if (idx[i] == k) members.push_back((int)i);
                if (members.empty()) continue;

                int new_med = med[k];
                double best_sum = std::numeric_limits<double>::infinity();
                for (int cand : members) {
                    double s = 0.0;
                    for (int p : members) {
                        s += dist_pair(&Xv[(size_t)cand * D],
                                       &Xv[(size_t)p * D], D, m);
                    }
                    if (s < best_sum) { best_sum = s; new_med = cand; }
                }
                if (new_med != med[k]) { med[k] = new_med; changed = true; }
            }
            if (!changed) break;
        }

        // Re-evaluate total against final medoids.
        double total = 0.0;
        std::fill(sumd.begin(), sumd.end(), 0.0);
        for (size_t i = 0; i < N; ++i) {
            const int k = idx[i];
            const double d = dist_pair(&Xv[i * D], &Xv[(size_t)med[k] * D], D, m);
            sumd[k] += d;
            total += d;
        }

        if (total < best_total) {
            best_total = total;
            std::copy(med.begin(),  med.end(),  best_med.begin());
            std::copy(idx.begin(),  idx.end(),  best_idx.begin());
            std::copy(sumd.begin(), sumd.end(), best_sumd.begin());
            best_iters = rep_iters;
            best_rep   = rep + 1;  // 1-based for MATLAB parity
        }
    }

    KmedoidsResult R{};

    R.idx = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    {
        double *ip = R.idx.doubleDataMut();
        for (size_t i = 0; i < N; ++i) ip[i] = double(best_idx[i] + 1);
    }

    R.C = Value::matrix(K, D, ValueType::DOUBLE, mr);
    {
        double *mp = R.C.doubleDataMut();
        for (int k = 0; k < K; ++k)
            for (size_t d = 0; d < D; ++d)
                mp[d * K + k] = Xv[(size_t)best_med[k] * D + d];
    }

    R.sumd = Value::matrix(K, 1, ValueType::DOUBLE, mr);
    {
        double *sp = R.sumd.doubleDataMut();
        for (int k = 0; k < K; ++k) sp[k] = best_sumd[k];
    }

    // 4th output D — N×K distance from each point to each medoid.
    R.D = Value::matrix(N, K, ValueType::DOUBLE, mr);
    {
        double *dp = R.D.doubleDataMut();
        for (size_t i = 0; i < N; ++i)
            for (int k = 0; k < K; ++k)
                dp[k * N + i] = dist_pair(&Xv[i * D],
                                          &Xv[(size_t)best_med[k] * D], D, m);
    }

    // 5th output midx — K×1 row index (1-based) of each medoid in X.
    R.midx = Value::matrix(K, 1, ValueType::DOUBLE, mr);
    {
        double *xp = R.midx.doubleDataMut();
        for (int k = 0; k < K; ++k) xp[k] = double(best_med[k] + 1);
    }

    R.iters    = best_iters;
    R.best_rep = best_rep;
    return R;
}

// Backward-compat 3-output wrapper.
std::tuple<Value, Value, Value>
kmedoids(std::pmr::memory_resource *mr, const Value &X, int K,
         int max_iter, int replicates, const std::string &metric_name)
{
    auto R = kmedoids_full(mr, X, K, max_iter, replicates, metric_name);
    return std::make_tuple(std::move(R.idx), std::move(R.C),
                           std::move(R.sumd));
}

// ════════════════════════════════════════════════════════════════════
// dbscan
// ════════════════════════════════════════════════════════════════════

std::tuple<Value, Value>
dbscan(std::pmr::memory_resource *mr, const Value &X,
       double eps, int minpts, const std::string &metric_name,
       double p)
{
    if (eps <= 0.0)  throw Error("dbscan: eps must be positive",
                                 0, 0, "dbscan", "", "m:dbscan:badeps");
    if (minpts <= 0) minpts = 1;
    const Metric m = parse_metric(metric_name);

    // For 'precomputed' the input is the N×N pairwise distance matrix
    // directly. Otherwise X is N×D feature matrix.
    const bool precomputed = (m == Metric::Precomputed);
    const size_t N = X.dims().rows();

    ScratchArena scratch(mr);
    ScratchVec<double> Xv(&scratch);
    ScratchVec<double> Dmat(&scratch);  // N×N row-major (precomputed only)
    size_t D = 0;
    if (precomputed) {
        if (X.dims().cols() != N)
            throw Error("dbscan: precomputed distance matrix must be N×N",
                        0, 0, "dbscan", "", "m:dbscan:badprecomp");
        Dmat.assign(N * N, 0.0);
        for (size_t i = 0; i < N; ++i)
            for (size_t j = 0; j < N; ++j)
                Dmat[i * N + j] = X.elemAsDouble(j * N + i);
    } else {
        D = X.dims().cols();
        Xv = read_rows(X, &scratch);
    }

    // Neighbour lookup. Returns count and writes indices into `out`.
    auto neighbours = [&](size_t i, ScratchVec<int> &out) {
        out.clear();
        for (size_t j = 0; j < N; ++j) {
            if (j == i) { out.push_back((int)j); continue; }
            double d;
            if (precomputed) d = Dmat[i * N + j];
            else             d = dist_pair(&Xv[i * D], &Xv[j * D], D, m, p);
            if (d <= eps) out.push_back((int)j);
        }
    };

    ScratchVec<int>     labels(N, 0, &scratch);   // 0 = unclassified
    ScratchVec<uint8_t> core(N, 0, &scratch);
    int cluster = 0;

    ScratchVec<int> nbrs(&scratch);
    ScratchVec<int> qn(&scratch);
    ScratchVec<int> seeds(&scratch);
    nbrs.reserve(64); qn.reserve(64); seeds.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        if (labels[i] != 0) continue;
        neighbours(i, nbrs);
        if ((int)nbrs.size() < minpts) {
            labels[i] = -1;  // noise (MATLAB R2025b convention)
            continue;
        }
        ++cluster;
        labels[i] = cluster;
        core[i] = 1;

        // Expand seed set (FIFO via index).
        seeds.clear();
        seeds.insert(seeds.end(), nbrs.begin(), nbrs.end());
        for (size_t s = 0; s < seeds.size(); ++s) {
            const int q = seeds[s];
            if ((size_t)q == i) continue;
            if (labels[q] == -1) labels[q] = cluster;  // noise → border
            if (labels[q] != 0) continue;              // already assigned
            labels[q] = cluster;
            neighbours((size_t)q, qn);
            if ((int)qn.size() >= minpts) {
                core[q] = 1;
                for (int n : qn) {
                    if (labels[n] == 0 || labels[n] == -1) seeds.push_back(n);
                }
            }
        }
    }

    Value idx = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *ip = idx.doubleDataMut();
    for (size_t i = 0; i < N; ++i) ip[i] = double(labels[i]);

    Value core_v = Value::matrix(N, 1, ValueType::LOGICAL, mr);
    uint8_t *cp = core_v.logicalDataMut();
    for (size_t i = 0; i < N; ++i) cp[i] = core[i];

    return std::make_tuple(std::move(idx), std::move(core_v));
}

// Backward-compat without p.
std::tuple<Value, Value>
dbscan(std::pmr::memory_resource *mr, const Value &X,
       double eps, int minpts, const std::string &metric_name)
{
    return dbscan(mr, X, eps, minpts, metric_name, 2.0);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void kmedoids_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kmedoids: requires (X, K[, N-V pairs])",
                    0, 0, "kmedoids", "", "m:kmedoids:nargin");
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
    auto R = kmedoids_full(ctx.engine->resource(), args[0], K,
                           max_iter, replicates, metric);
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
                    0, 0, "dbscan", "", "m:dbscan:nargin");
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
    auto [idx, core] = dbscan(ctx.engine->resource(), args[0],
                              eps, minpts, metric, p);
    outs[0] = std::move(idx);
    if (nargout > 1) outs[1] = std::move(core);
}

} // namespace detail
} // namespace numkit::stats
