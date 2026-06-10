// toolboxes/stats/src/distributions/hypergeom.cpp
//
// Hypergeometric distribution: drawing N items without replacement from a
// population of M with K successes. f(k; M, K, N) = C(K, k)·C(M-K, N-k)/C(M, N).

#include <numkit/stats/distributions/hypergeom.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "hypergeom_detail.hpp"

namespace numkit::stats {


Value hygepdf(const Value &k, double M, double K, double N, std::pmr::memory_resource *mr)
{
    if (!params_valid(M, K, N))
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return hyge_pmf(ki, M, K, N); }, mr);
}

Value hygecdf(const Value &k, double M, double K, double N, std::pmr::memory_resource *mr)
{
    if (!params_valid(M, K, N))
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return hyge_cdf_scalar(ki, M, K, N); }, mr);
}

Value hygeinv(const Value &q, double M, double K, double N, std::pmr::memory_resource *mr)
{
    if (!params_valid(M, K, N))
        return elementwise(q, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(q, [=](double qi){ return hyge_inv_scalar(qi, M, K, N); }, mr);
}

Value hygernd(double M, double K, double N, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (!params_valid(M, K, N) || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    // Inverse-cdf walk per draw. M, K, N fixed so the walk is fast.
    std::uniform_real_distribution<double> ud(0.0, 1.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < cnt; ++i) od[i] = hyge_inv_scalar(ud(gen), M, K, N);
    return out;
}

std::tuple<double, double> hygestat(double M, double K, double N)
{
    if (!params_valid(M, K, N) || M < 1.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double mean = N * K / M;
    if (M < 2.0) return std::make_tuple(mean, std::numeric_limits<double>::quiet_NaN());
    const double var = N * K * (M - K) * (M - N) / (M * M * (M - 1.0));
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
