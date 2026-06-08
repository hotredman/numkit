// toolboxes/stats/src/cluster/kmeans.cpp
//
// Lloyd's algorithm with k-means++ initialisation. Single-threaded
// portable implementation; SIMD / parallel variants planned later.

#include <numkit/stats/cluster/kmeans.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <mutex>
#include <random>

#include "kmeans_detail.hpp"

namespace numkit::stats {


// Full kmeans with optional 4th D output (N×K squared distances).

KmeansResult
kmeans_full(const Value &X, int K, int max_iter, int replicates, std::pmr::memory_resource *mr)
{
    if (max_iter <= 0)   max_iter = 100;
    if (replicates <= 0) replicates = 1;

    const size_t N = X.dims().rows();
    const size_t D = X.dims().cols();
    if (K < 1 || (size_t)K > N)
        throw Error("kmeans: K must be in 1..N", 0, 0, "kmeans", "",
                    "numkit:kmeans:badK");

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

} // namespace numkit::stats
