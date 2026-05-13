// libs/stats/src/cluster/kmeans.cpp
//
// Lloyd's algorithm with k-means++ initialisation. Single-threaded
// portable implementation; SIMD / parallel variants planned later.

#include <numkit/stats/cluster/kmeans.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <mutex>
#include <random>

namespace numkit::stats {

namespace {

inline double sq_dist(const double *a, const double *b, size_t D) {
    double s = 0.0;
    for (size_t k = 0; k < D; ++k) { double d = a[k] - b[k]; s += d * d; }
    return s;
}

// k-means++ centroid initialisation. Returns K rows of D values each
// (flattened K*D-length buffer).
ScratchVec<double> kmeanspp_init(const ScratchVec<double> &X,
                                 size_t N, size_t D, int K,
                                 numkit::builtin::detail::MatlabMT19937 &gen,
                                 std::pmr::memory_resource *scratch_mr)
{
    ScratchVec<double> C((size_t)K * D, scratch_mr);
    std::uniform_int_distribution<size_t> first(0, N - 1);
    const size_t i0 = first(gen);
    std::copy(X.begin() + i0 * D, X.begin() + (i0 + 1) * D, C.begin());

    ScratchVec<double> dist(N, std::numeric_limits<double>::infinity(),
                            scratch_mr);
    for (int k = 1; k < K; ++k) {
        // Update each point's nearest-centroid distance against the new
        // centroid (k-1).
        const double *cprev = &C[(size_t)(k - 1) * D];
        for (size_t i = 0; i < N; ++i) {
            const double d = sq_dist(&X[i * D], cprev, D);
            if (d < dist[i]) dist[i] = d;
        }
        // Sample next centroid with probability proportional to dist.
        double total = 0.0;
        for (auto v : dist) total += v;
        if (total <= 0.0) {
            // All points coincide with existing centroids; pick uniformly.
            const size_t pick = first(gen);
            std::copy(X.begin() + pick * D, X.begin() + (pick + 1) * D,
                      C.begin() + (size_t)k * D);
            continue;
        }
        std::uniform_real_distribution<double> ud(0.0, total);
        const double r = ud(gen);
        double acc = 0.0;
        size_t pick = N - 1;
        for (size_t i = 0; i < N; ++i) {
            acc += dist[i];
            if (acc >= r) { pick = i; break; }
        }
        std::copy(X.begin() + pick * D, X.begin() + (pick + 1) * D,
                  C.begin() + (size_t)k * D);
    }
    return C;
}

// Lloyd iteration. Returns (assignments, centroids, sum-of-squared-dist
// per cluster, total-WCSS). Stops when no assignment changes or when
// max_iter reached.
struct LloydResult {
    ScratchVec<int>    idx;
    ScratchVec<double> C;
    ScratchVec<double> sumd;
    double             total{};

    LloydResult(std::pmr::memory_resource *scratch_mr)
        : idx(scratch_mr), C(scratch_mr), sumd(scratch_mr) {}
};

LloydResult lloyd(const ScratchVec<double> &X, size_t N, size_t D, int K,
                  ScratchVec<double> &&C0, int max_iter,
                  std::pmr::memory_resource *scratch_mr)
{
    LloydResult res(scratch_mr);
    res.idx.assign(N, 0);
    res.C    = std::move(C0);
    res.sumd.assign(K, 0.0);
    res.total = 0.0;

    ScratchVec<int>    counts((size_t)K, 0, scratch_mr);
    ScratchVec<double> Cnext((size_t)K * D, 0.0, scratch_mr);

    for (int iter = 0; iter < max_iter; ++iter) {
        // Assign each point to nearest centroid.
        bool changed = false;
        std::fill(counts.begin(), counts.end(), 0);
        std::fill(Cnext.begin(), Cnext.end(), 0.0);
        std::fill(res.sumd.begin(), res.sumd.end(), 0.0);
        res.total = 0.0;

        for (size_t i = 0; i < N; ++i) {
            const double *xi = &X[i * D];
            int best = 0;
            double bd = std::numeric_limits<double>::infinity();
            for (int k = 0; k < K; ++k) {
                const double d = sq_dist(xi, &res.C[(size_t)k * D], D);
                if (d < bd) { bd = d; best = k; }
            }
            if (res.idx[i] != best) { res.idx[i] = best; changed = true; }
            counts[best] += 1;
            for (size_t k = 0; k < D; ++k) Cnext[(size_t)best * D + k] += xi[k];
            res.sumd[best] += bd;
            res.total += bd;
        }
        // Update centroids.
        for (int k = 0; k < K; ++k) {
            if (counts[k] > 0) {
                const double inv = 1.0 / double(counts[k]);
                for (size_t d = 0; d < D; ++d)
                    res.C[(size_t)k * D + d] = Cnext[(size_t)k * D + d] * inv;
            }
            // Empty cluster: leave centroid where it was (could re-seed,
            // but Lloyd-classic keeps it).
        }
        if (!changed) break;
    }
    // Final pass: recompute sumd against the current (post-update) centroids
    // so the reported WCSS reflects the converged solution.
    std::fill(res.sumd.begin(), res.sumd.end(), 0.0);
    res.total = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double *xi = &X[i * D];
        const int k = res.idx[i];
        const double d = sq_dist(xi, &res.C[(size_t)k * D], D);
        res.sumd[k] += d;
        res.total   += d;
    }
    return res;
}

} // anonymous

// Full kmeans with optional 4th D output (N×K squared distances).
struct KmeansResult {
    Value idx;
    Value C;
    Value sumd;
    Value D;     // optional N×K
};

KmeansResult
kmeans_full(const Value &X, int K, int max_iter, int replicates, std::pmr::memory_resource *mr)
{
    if (max_iter <= 0)   max_iter = 100;
    if (replicates <= 0) replicates = 1;

    const size_t N = X.dims().rows();
    const size_t D = X.dims().cols();
    if (K < 1 || (size_t)K > N)
        throw Error("kmeans: K must be in 1..N", 0, 0, "kmeans", "",
                    "m:kmeans:badK");

    ScratchArena scratch(mr);

    // Read X into a flat row-major scratch buffer.
    ScratchVec<double> Xv(N * D, &scratch);
    for (size_t r = 0; r < N; ++r)
        for (size_t c = 0; c < D; ++c)
            Xv[r * D + c] = X.elemAsDouble(c * N + r);

    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();

    LloydResult best(&scratch);
    double best_total = std::numeric_limits<double>::infinity();

    for (int rep = 0; rep < replicates; ++rep) {
        ScratchVec<double> C0(&scratch);
        {
            std::lock_guard<std::mutex> lk(mtx);
            C0 = kmeanspp_init(Xv, N, D, K, gen, &scratch);
        }
        LloydResult res = lloyd(Xv, N, D, K, std::move(C0), max_iter, &scratch);
        if (res.total < best_total) {
            best_total = res.total;
            best = std::move(res);
        }
    }

    KmeansResult out;

    out.idx = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    {
        double *ip = out.idx.doubleDataMut();
        for (size_t i = 0; i < N; ++i) ip[i] = double(best.idx[i] + 1);
    }

    out.C = Value::matrix(K, D, ValueType::DOUBLE, mr);
    {
        double *cp = out.C.doubleDataMut();
        for (int k = 0; k < K; ++k)
            for (size_t d = 0; d < D; ++d)
                cp[d * K + k] = best.C[(size_t)k * D + d];
    }

    out.sumd = Value::matrix(K, 1, ValueType::DOUBLE, mr);
    {
        double *sp = out.sumd.doubleDataMut();
        for (int k = 0; k < K; ++k) sp[k] = best.sumd[k];
    }

    // 4th output D — N×K squared distance from each point to each centroid.
    out.D = Value::matrix(N, K, ValueType::DOUBLE, mr);
    {
        double *dp = out.D.doubleDataMut();
        for (size_t i = 0; i < N; ++i)
            for (int k = 0; k < K; ++k)
                dp[k * N + i] = sq_dist(&Xv[i * D], &best.C[(size_t)k * D], D);
    }

    return out;
}

// Backward-compat 3-output wrapper.
std::tuple<Value, Value, Value>
kmeans(const Value &X, int K, int max_iter, int replicates, std::pmr::memory_resource *mr)
{
    auto R = kmeans_full(X, K, max_iter, replicates, mr);
    return std::make_tuple(std::move(R.idx), std::move(R.C),
                           std::move(R.sumd));
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void kmeans_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kmeans: requires (X, K[, N-V pairs])",
                    0, 0, "kmeans", "", "m:kmeans:nargin");
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
                            0, 0, "kmeans", "", "m:kmeans:distance");
        }
        else if (key == "start") {
            const std::string sn = lower(v.toString());
            if (sn != "plus")
                throw Error("kmeans: only Start = 'plus' is implemented "
                            "(got '" + sn + "')",
                            0, 0, "kmeans", "", "m:kmeans:start");
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
