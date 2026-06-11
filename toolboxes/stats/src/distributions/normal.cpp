// toolboxes/stats/src/distributions/normal.cpp
//
// Normal distribution. Standard formulas:
//   pdf:  f(x; μ, σ) = (1/(σ√(2π))) · exp(-(x-μ)² / (2σ²))
//   cdf:  F(x; μ, σ) = ½ · [1 + erf((x-μ) / (σ√2))]
//   icdf: F⁻¹(p; μ, σ) = μ + σ·√2·erfinv(2p - 1)
//   rnd:  μ + σ · randn
//   stat: [mean=μ, var=σ²]

#include <numkit/stats/distributions/normal.hpp>

#include <numkit/math/random/rng.hpp>          // sharedEngine, randn
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"            // stripUpperFlag / applyUpperInPlace

#include <cmath>
#include <cstring>
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "normal_detail.hpp"

namespace numkit::stats {


Value normpdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr)
{
    return elementwise(x, [=](double xi) { return normpdfK(xi, mu, sigma); }, mr);
}

Value normcdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr)
{
    return elementwise(x, [=](double xi) { return normcdfK(xi, mu, sigma); }, mr);
}

Value norminv(const Value &p, double mu, double sigma, std::pmr::memory_resource *mr)
{
    // Boundary p ∈ {0, 1} → ±Inf; out-of-range or NaN p → NaN; sigma<=0 → NaN
    // (matches MATLAB R2025b — Octave too). See norminvK.
    return elementwise(p, [=](double pi) { return norminvK(pi, mu, sigma); }, mr);
}

Value normrnd(double mu, double sigma, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    if (sigma < 0.0) sigma = 0.0;
    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::normal_distribution<double> nd(mu, sigma);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = nd(gen);
    return out;
}

std::tuple<double, double> normstat(double mu, double sigma)
{
    // MATLAB convention: sigma <= 0 ⇒ NaN/NaN (matches Octave too).
    if (sigma <= 0.0) {
        const double NaN = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(NaN, NaN);
    }
    return std::make_tuple(mu, sigma * sigma);
}

} // namespace numkit::stats
