// toolboxes/stats/src/distributions/nakagami.cpp

#include <numkit/stats/distributions/nakagami.hpp>

#include <numkit/builtin/math/special/special.hpp>   // gammainc, gammaincinv
#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "nakagami_detail.hpp"

namespace numkit::stats {


Value nakapdf(const Value &x, double mu, double omega, std::pmr::memory_resource *mr)
{
    if (mu <= 0.0 || omega <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // Pre-compute constants
    const double C = 2.0 * std::pow(mu, mu) / (std::tgamma(mu) * std::pow(omega, mu));
    const double k = mu / omega;
    return elementwise(x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) return (mu < 0.5) ? std::numeric_limits<double>::infinity()
                                         : (mu == 0.5 ? C : 0.0);
        return C * std::pow(xi, 2.0 * mu - 1.0) * std::exp(-k * xi * xi);
    }, mr);
}

Value nakacdf(const Value &x, double mu, double omega, std::pmr::memory_resource *mr)
{
    if (mu <= 0.0 || omega <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double k = mu / omega;
    return elementwise(x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return gammainc_scalar(k * xi * xi, mu, mr);
    }, mr);
}

Value nakainv(const Value &p, double mu, double omega, std::pmr::memory_resource *mr)
{
    if (mu <= 0.0 || omega <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double s = std::sqrt(omega / mu);
    return elementwise(p, [=](double pi) {
        if (!(pi >= 0.0 && pi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        const double q = gammaincinv_scalar(pi, mu, mr);
        return s * std::sqrt(q);
    }, mr);
}

Value nakarnd(double mu, double omega, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (mu <= 0.0 || omega <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // X² ~ Gamma(shape=mu, scale=omega/mu) ⇒ X = √Gamma.
    std::gamma_distribution<double> gd(mu, omega / mu);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = std::sqrt(gd(gen));
    return out;
}

std::tuple<double, double> nakastat(double mu, double omega)
{
    if (mu <= 0.0 || omega <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double r = std::tgamma(mu + 0.5) / std::tgamma(mu);
    const double mean = std::sqrt(omega / mu) * r;
    const double var  = omega * (1.0 - r * r / mu);
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
