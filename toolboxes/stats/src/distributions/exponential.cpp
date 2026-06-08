// toolboxes/stats/src/distributions/exponential.cpp

#include <numkit/stats/distributions/exponential.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "exponential_detail.hpp"

namespace numkit::stats {


Value exppdf(const Value &x, double mu, std::pmr::memory_resource *mr)
{
    return elementwise(x, [=](double xi) { return exppdfK(xi, mu); }, mr);
}

Value expcdf(const Value &x, double mu, std::pmr::memory_resource *mr)
{
    return elementwise(x, [=](double xi) { return expcdfK(xi, mu); }, mr);
}

Value expinv(const Value &p, double mu, std::pmr::memory_resource *mr)
{
    return elementwise(p, [=](double pi) { return expinvK(pi, mu); }, mr);
}

Value exprnd(double mu, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (mu <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::exponential_distribution<double> ed(1.0 / mu);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = ed(gen);
    return out;
}

std::tuple<double, double> expstat(double mu)
{
    if (mu <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(mu, mu * mu);
}

} // namespace numkit::stats
