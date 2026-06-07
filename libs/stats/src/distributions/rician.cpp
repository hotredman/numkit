// libs/stats/src/distributions/rician.cpp

#include <numkit/stats/distributions/rician.hpp>

#include <numkit/builtin/math/special/special.hpp>   // besseli
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/comm/channel/channel.hpp>           // marcumq

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

#include "rician_detail.hpp"

namespace numkit::stats {


Value ricepdf(const Value &x, double s, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0 || s < 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double s2 = s * s;
    const double sg2 = sigma * sigma;
    return elementwise(x, [&](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) {
            // Limit: at x=0 the PDF is 0 unless s=0 (Rayleigh case).
            return (s == 0.0) ? 0.0 : 0.0;
        }
        const double arg = xi * s / sg2;
        const double i0  = besseli_scalar(0.0, arg, mr);
        return (xi / sg2) * std::exp(-(xi * xi + s2) / (2.0 * sg2)) * i0;
    }, mr);
}

Value ricecdf(const Value &x, double s, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0 || s < 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(x, [&](double xi) {
        if (xi <= 0.0) return 0.0;
        return 1.0 - marcumq_scalar(s / sigma, xi / sigma, mr);
    }, mr);
}

Value riceinv(const Value &p, double s, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0 || s < 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [&](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        // Bracket then bisect: F(0) = 0, F(s + 8σ) ≈ 1 (Gaussian-tail bound).
        double lo = 0.0;
        double hi = s + 8.0 * sigma;
        // Expand `hi` if needed.
        for (int iter = 0; iter < 50; ++iter) {
            const double Fhi = 1.0 - marcumq_scalar(s / sigma, hi / sigma, mr);
            if (Fhi >= pi) break;
            hi *= 2.0;
        }
        // Bisection to ~1e-12 relative tolerance.
        for (int iter = 0; iter < 80; ++iter) {
            const double mid = 0.5 * (lo + hi);
            if (mid == lo || mid == hi) return mid;
            const double F = 1.0 - marcumq_scalar(s / sigma, mid / sigma, mr);
            if (F < pi) lo = mid; else hi = mid;
        }
        return 0.5 * (lo + hi);
    }, mr);
}

Value ricernd(double s, double sigma, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || s < 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::normal_distribution<double> nd1(s, sigma);
    std::normal_distribution<double> nd2(0.0, sigma);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const double a = nd1(gen);
        const double b = nd2(gen);
        od[i] = std::sqrt(a * a + b * b);
    }
    return out;
}

std::tuple<double, double> ricestat(double s, double sigma, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0 || s < 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    if (s == 0.0) {
        return std::make_tuple(sigma * std::sqrt(M_PI / 2.0),
                               sigma * sigma * (2.0 - M_PI / 2.0));
    }
    const double s2 = s * s;
    const double sg2 = sigma * sigma;
    const double z = -s2 / (2.0 * sg2);
    const double half = s2 / (4.0 * sg2);
    const double i0 = besseli_scalar(0.0, half, mr);
    const double i1 = besseli_scalar(1.0, half, mr);
    const double L = std::exp(z / 2.0) * ((1.0 - z) * i0 - z * i1);
    const double mean = sigma * std::sqrt(M_PI / 2.0) * L;
    const double var  = 2.0 * sg2 + s2 - mean * mean;
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
