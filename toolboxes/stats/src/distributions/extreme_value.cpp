// toolboxes/stats/src/distributions/extreme_value.cpp

#include <numkit/stats/distributions/extreme_value.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "extreme_value_detail.hpp"

namespace numkit::stats {


Value evpdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_s = 1.0 / sigma;
    return elementwise(x, [=](double xi) {
        const double t = (xi - mu) * inv_s;
        return inv_s * std::exp(t - std::exp(t));
    }, mr);
}

Value evcdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(x, [=](double xi) {
        const double t = (xi - mu) / sigma;
        return -std::expm1(-std::exp(t));
    }, mr);
}

Value evinv(const Value &p, double mu, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return -std::numeric_limits<double>::infinity();
        if (pi >= 1.0) return  std::numeric_limits<double>::infinity();
        // x = mu + sigma · log(-log1p(-p))
        return mu + sigma * std::log(-std::log1p(-pi));
    }, mr);
}

Value evrnd(double mu, double sigma, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        // genRes53 — MATLAB-canonical 53-bit uniform in [0, 1). Direct
        // call (bypasses std::uniform_real_distribution whose uint32->
        // double mapping is implementation-defined and breaks parity).
        const double u = gen.genRes53();
        // Avoid log(0) for u==0; genRes53 can return exactly 0 (low
        // probability) — guard.
        const double safe = (u > 0.0) ? u : std::numeric_limits<double>::min();
        // MATLAB convention: evrnd is Gumbel-MIN (Type-I extreme value
        // for minima), so x = mu + sigma * log(-log(u)) -- direct u, NOT
        // 1-u. (We previously used Gumbel-MAX log(-log1p(-u)).)
        od[i] = mu + sigma * std::log(-std::log(safe));
    }
    return out;
}

std::tuple<double, double> evstat(double mu, double sigma)
{
    if (sigma <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(mu - sigma * kEulerGamma,
                           sigma * sigma * kPi2Over6);
}

} // namespace numkit::stats
