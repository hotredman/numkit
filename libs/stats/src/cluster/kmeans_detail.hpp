// libs/.../kmeans_detail.hpp — private compute substrate (anon-in-header,
// internal linkage per TU) shared by kmeans.cpp compute and kmeans_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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


struct KmeansResult {
    Value idx;
    Value C;
    Value sumd;
    Value D;     // optional N×K
};
KmeansResult kmeans_full(const Value &X, int K, int max_iter, int replicates,
                         std::pmr::memory_resource *mr);

} // namespace numkit::stats
