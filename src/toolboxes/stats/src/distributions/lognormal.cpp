// toolboxes/stats/src/distributions/lognormal.cpp

#include <numkit/stats/distributions/lognormal.hpp>

#include <numkit/math/random/rng.hpp>

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

#include "lognormal_detail.hpp"

namespace numkit::stats {


Value lognpdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_sig = 1.0 / sigma;
    const double inv_sqrt2pi = 1.0 / kSqrt2Pi;
    return elementwise(x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        const double z = (std::log(xi) - mu) * inv_sig;
        return inv_sqrt2pi * inv_sig * std::exp(-0.5 * z * z) / xi;
    }, mr);
}

Value logncdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_sig = 1.0 / sigma;
    return elementwise(x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return phi((std::log(xi) - mu) * inv_sig);
    }, mr);
}

Value logninv(const Value &p, double mu, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        if (std::isnan(pi) || pi < 0.0 || pi > 1.0)
            return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi == 1.0) return std::numeric_limits<double>::infinity();
        return std::exp(mu + sigma * phiInv(pi));
    }, mr);
}

Value lognrnd(::numkit::ops::RngContext &rng, double mu, double sigma, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = rng;
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::lognormal_distribution<double> ld(mu, sigma);
    for (size_t i = 0; i < n; ++i) od[i] = ld(gen);
    return out;
}

std::tuple<double, double> lognstat(double mu, double sigma)
{
    if (sigma <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double s2 = sigma * sigma;
    const double mean = std::exp(mu + 0.5 * s2);
    const double var = std::expm1(s2) * std::exp(2.0 * mu + s2);
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
