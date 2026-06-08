// toolboxes/stats/src/distributions/gp.cpp

#include <numkit/stats/distributions/gp.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "gp_detail.hpp"

namespace numkit::stats {


Value gppdf(const Value &x, double k, double sigma, double theta, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_s = 1.0 / sigma;
    return elementwise(x, [=](double xi) {
        const double z = (xi - theta) * inv_s;
        if (z < 0.0) return 0.0;
        if (k == 0.0) return inv_s * std::exp(-z);
        const double t = 1.0 + k * z;
        if (t <= 0.0) return 0.0;
        return inv_s * std::pow(t, -1.0 / k - 1.0);
    }, mr);
}

Value gpcdf(const Value &x, double k, double sigma, double theta, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    const double inv_s = 1.0 / sigma;
    return elementwise(x, [=](double xi) {
        const double z = (xi - theta) * inv_s;
        if (z <= 0.0) return 0.0;
        if (k == 0.0) return -std::expm1(-z);
        const double t = 1.0 + k * z;
        if (t <= 0.0) return (k > 0) ? 0.0 : 1.0;
        return 1.0 - std::pow(t, -1.0 / k);
    }, mr);
}

Value gpinv(const Value &p, double k, double sigma, double theta, std::pmr::memory_resource *mr)
{
    if (sigma <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        return gp_inv_one(pi, k, sigma, theta);
    }, mr);
}

Value gprnd(double k, double sigma, double theta, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (sigma <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        // genRes53 -- MATLAB-canonical 53-bit uniform.
        // gprnd uses u DIRECTLY (not 1-u) per MATLAB convention:
        //   x = theta + sigma * (u^(-k) - 1) / k       for k != 0
        //   x = theta - sigma * log(u)                  for k == 0
        // (gpinv(p) uses 1-p which is the standard ICDF; sampling
        // from rand() with the swapped-u form gives the same
        // distribution but matches MATLAB's specific bit sequence.)
        double u = gen.genRes53();
        if (u <= 0.0) u = std::numeric_limits<double>::min();
        if (k == 0.0) {
            od[i] = theta - sigma * std::log(u);
        } else {
            od[i] = theta + sigma * (std::pow(u, -k) - 1.0) / k;
        }
    }
    return out;
}

std::tuple<double, double> gpstat(double k, double sigma, double theta)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (sigma <= 0.0) return std::make_tuple(nan, nan);
    if (!(k < 1.0))
        return std::make_tuple(std::numeric_limits<double>::infinity(), nan);
    const double mean = theta + sigma / (1.0 - k);
    if (!(k < 0.5))
        return std::make_tuple(mean, std::numeric_limits<double>::infinity());
    const double var = sigma * sigma / ((1.0 - k) * (1.0 - k) * (1.0 - 2.0 * k));
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
