// toolboxes/stats/src/distributions/geometric.cpp

#include <numkit/stats/distributions/geometric.hpp>

#include <numkit/builtin/datafun.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "geometric_detail.hpp"

namespace numkit::stats {


Value geopdf(const Value &k, double p, std::pmr::memory_resource *mr)
{
    if (p <= 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki) {
        if (ki < 0.0 || std::floor(ki) != ki) return 0.0;
        if (p == 1.0) return ki == 0.0 ? 1.0 : 0.0;
        return std::pow(1.0 - p, ki) * p;
    }, mr);
}

Value geocdf(const Value &k, double p, std::pmr::memory_resource *mr)
{
    if (p <= 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki) {
        if (ki < 0.0) return 0.0;
        if (p == 1.0) return ki >= 0.0 ? 1.0 : 0.0;
        // F(k) = 1 - (1-p)^(⌊k⌋ + 1)
        return -std::expm1((std::floor(ki) + 1.0) * std::log1p(-p));
    }, mr);
}

Value geoinv(const Value &q, double p, std::pmr::memory_resource *mr)
{
    if (p <= 0.0 || p > 1.0)
        return elementwise(q, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(q, [=](double qi) {
        if (!(qi >= 0.0 && qi <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
        if (qi == 0.0) return 0.0;
        if (qi >= 1.0) return std::numeric_limits<double>::infinity();
        if (p == 1.0) return 0.0;
        // Smallest integer k such that 1 - (1-p)^(k+1) ≥ qi
        //   ⇔ (1-p)^(k+1) ≤ 1-qi
        //   ⇔ k+1 ≥ log(1-qi) / log(1-p)
        //   ⇔ k ≥ log(1-qi)/log(1-p) - 1
        const double v = std::log1p(-qi) / std::log1p(-p) - 1.0;
        double k = std::ceil(v);
        if (k < 0.0) k = 0.0;
        // One-ULP tolerance: if v - (k - 1) < tol, we already had k-1 satisfying.
        if (k > 0.0) {
            const double cdf_prev = -std::expm1(k * std::log1p(-p)); // F(k-1)
            const double tol = std::max(1e-13, qi * 1e-13);
            if (cdf_prev >= qi - tol) k -= 1.0;
        }
        return k;
    }, mr);
}

Value geornd(::numkit::ops::RngContext &rng, double p, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = rng;
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (p <= 0.0 || p > 1.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    std::geometric_distribution<int> gd(p);
    for (size_t i = 0; i < cnt; ++i) od[i] = static_cast<double>(gd(gen));
    return out;
}

std::tuple<double, double> geostat(double p)
{
    if (p <= 0.0 || p > 1.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double q = 1.0 - p;
    return std::make_tuple(q / p, q / (p * p));
}

} // namespace numkit::stats
