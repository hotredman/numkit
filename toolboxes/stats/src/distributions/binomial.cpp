// toolboxes/stats/src/distributions/binomial.cpp

#include <numkit/stats/distributions/binomial.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "binomial_detail.hpp"

namespace numkit::stats {


Value binopdf(const Value &k, double n, double p, std::pmr::memory_resource *mr)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return bino_pmf(ki, n, p); }, mr);
}

Value binocdf(const Value &k, double n, double p, std::pmr::memory_resource *mr)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return bino_cdf_scalar(ki, n, p, mr); }, mr);
}

Value binoinv(const Value &p_in, double n, double p, std::pmr::memory_resource *mr)
{
    if (!bino_params_ok(n, p))
        return elementwise(p_in, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p_in, [=](double pi) { return bino_inv_scalar(pi, n, p); }, mr);
}

Value binornd(double n, double p, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    std::binomial_distribution<int> bd(static_cast<int>(n), p);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < cnt; ++i) od[i] = static_cast<double>(bd(gen));
    return out;
}

std::tuple<double, double> binostat(double n, double p)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(n * p, n * p * (1.0 - p));
}

} // namespace numkit::stats
