// toolboxes/stats/src/distributions/gev.cpp

#include <numkit/stats/distributions/gev.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "gev_detail.hpp"

namespace numkit::stats {


Value gevpdf(const Value &x, double k, double sigma, double mu, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_s = 1.0 / sigma;
    return elementwise(x, [=](double xi) {
        const double z = (xi - mu) * inv_s;
        if (k == 0.0) return inv_s * std::exp(-z - std::exp(-z));
        const double t = 1.0 + k * z;
        if (t <= 0.0) return 0.0;
        const double tinvk = std::pow(t, -1.0 / k);
        return inv_s * std::pow(t, -1.0 / k - 1.0) * std::exp(-tinvk);
    }, mr);
}

Value gevcdf(const Value &x, double k, double sigma, double mu, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_s = 1.0 / sigma;
    return elementwise(x, [=](double xi) {
        const double z = (xi - mu) * inv_s;
        if (k == 0.0) return std::exp(-std::exp(-z));
        const double t = 1.0 + k * z;
        if (t <= 0.0) {
            // For k > 0 below the lower endpoint: F = 0.
            // For k < 0 above the upper endpoint: F = 1.
            return (k > 0) ? 0.0 : 1.0;
        }
        return std::exp(-std::pow(t, -1.0 / k));
    }, mr);
}

Value gevinv(const Value &p, double k, double sigma, double mu, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        return gev_inv_one(pi, k, sigma, mu);
    }, mr);
}

Value gevrnd(double k, double sigma, double mu, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        // genRes53 -- MATLAB-canonical 53-bit uniform; bypasses
        // std::uniform_real_distribution to preserve bit-parity.
        double u = gen.genRes53();
        if (u <= 0.0) u = std::numeric_limits<double>::min();
        od[i] = gev_inv_one(u, k, sigma, mu);
    }
    return out;
}

std::tuple<double, double> gevstat(double k, double sigma, double mu)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (sigma <= 0.0) return std::make_tuple(nan, nan);
    if (k == 0.0)
        return std::make_tuple(mu + sigma * kEulerGamma, sigma * sigma * kPi2Over6);
    if (!(k < 1.0))
        return std::make_tuple(std::numeric_limits<double>::infinity(), nan);
    const double g1 = std::tgamma(1.0 - k);
    const double mean = mu + sigma * (g1 - 1.0) / k;
    if (!(k < 0.5))
        return std::make_tuple(mean, std::numeric_limits<double>::infinity());
    const double g2 = std::tgamma(1.0 - 2.0 * k);
    const double var = (sigma * sigma / (k * k)) * (g2 - g1 * g1);
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
