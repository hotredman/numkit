// libs/stats/src/distributions/ncx2.cpp

#include <numkit/stats/distributions/ncx2.hpp>

#include <numkit/builtin/math/special/special.hpp>   // besseli, gammainc
#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "ncx2_detail.hpp"

namespace numkit::stats {


Value ncx2pdf(const Value &x, double k, double lambda, std::pmr::memory_resource *mr)
{
    if (k <= 0.0 || lambda < 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    if (lambda == 0.0) {
        // Reduce to chi²(k).
        const double C = 1.0 / (std::pow(2.0, k / 2.0) * std::tgamma(k / 2.0));
        return elementwise(x, [=](double xi) {
            if (xi <= 0.0) return (k == 2.0 && xi == 0.0) ? 0.5 : 0.0;
            return C * std::pow(xi, k / 2.0 - 1.0) * std::exp(-xi / 2.0);
        }, mr);
    }
    const double nu = (k - 2.0) / 2.0;
    return elementwise(x, [&](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) return 0.0;
        const double sqrtLx = std::sqrt(lambda * xi);
        const double iv = besseli_scalar(nu, sqrtLx, mr);
        return 0.5 * std::exp(-(xi + lambda) / 2.0)
             * std::pow(xi / lambda, (k - 2.0) / 4.0)
             * iv;
    }, mr);
}

Value ncx2cdf(const Value &x, double k, double lambda, std::pmr::memory_resource *mr)
{
    if (k <= 0.0 || lambda < 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(x, [&](double xi) {
        return ncx2cdf_one(xi, k, lambda, mr);
    }, mr);
}

Value ncx2inv(const Value &p, double k, double lambda, std::pmr::memory_resource *mr)
{
    if (k <= 0.0 || lambda < 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [&](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        // Bracket on [0, mean + 10·sqrt(var)] then bisect.
        const double mean = k + lambda;
        const double var  = 2.0 * (k + 2.0 * lambda);
        double lo = 0.0;
        double hi = mean + 10.0 * std::sqrt(var) + 1.0;
        for (int iter = 0; iter < 50; ++iter) {
            if (ncx2cdf_one(hi, k, lambda, mr) >= pi) break;
            hi *= 2.0;
        }
        for (int iter = 0; iter < 80; ++iter) {
            const double mid = 0.5 * (lo + hi);
            if (mid == lo || mid == hi) return mid;
            const double F = ncx2cdf_one(mid, k, lambda, mr);
            if (F < pi) lo = mid; else hi = mid;
        }
        return 0.5 * (lo + hi);
    }, mr);
}

Value ncx2rnd(double k, double lambda, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (k <= 0.0 || lambda < 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // Draw J ~ Poisson(λ/2), then X ~ chi²(k + 2J) = Gamma(shape=k/2+J, scale=2).
    std::poisson_distribution<int> pd(lambda / 2.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const int J = pd(gen);
        std::gamma_distribution<double> gd(k / 2.0 + double(J), 2.0);
        od[i] = gd(gen);
    }
    return out;
}

std::tuple<double, double> ncx2stat(double k, double lambda)
{
    if (k <= 0.0 || lambda < 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(k + lambda, 2.0 * (k + 2.0 * lambda));
}

} // namespace numkit::stats
