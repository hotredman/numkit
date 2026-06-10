// toolboxes/stats/src/distributions/rayleigh.cpp

#include <numkit/stats/distributions/rayleigh.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "rayleigh_detail.hpp"

namespace numkit::stats {


Value raylpdf(const Value &x, double b, std::pmr::memory_resource *mr)
{
    if (b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_b2 = 1.0 / (b * b);
    return elementwise(x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        return xi * inv_b2 * std::exp(-0.5 * xi * xi * inv_b2);
    }, mr);
}

Value raylcdf(const Value &x, double b, std::pmr::memory_resource *mr)
{
    if (b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_b2 = 1.0 / (b * b);
    return elementwise(x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return -std::expm1(-0.5 * xi * xi * inv_b2);
    }, mr);
}

Value raylinv(const Value &p, double b, std::pmr::memory_resource *mr)
{
    if (b <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        if (pi < 0.0 || pi > 1.0) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        return b * std::sqrt(-2.0 * std::log1p(-pi));
    }, mr);
}

Value raylrnd(double b, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (b <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // Inverse-cdf sampling: X = b·sqrt(-2 log U), U ~ U(0,1).
    std::uniform_real_distribution<double> ud(0.0, 1.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        double u;
        do { u = ud(gen); } while (u == 0.0);
        od[i] = b * std::sqrt(-2.0 * std::log(u));
    }
    return out;
}

std::tuple<double, double> raylstat(double b)
{
    if (b <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double mean = b * std::sqrt(0.5 * M_PI);
    const double var  = (2.0 - 0.5 * M_PI) * b * b;
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
