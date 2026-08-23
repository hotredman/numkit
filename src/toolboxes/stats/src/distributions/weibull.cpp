// toolboxes/stats/src/distributions/weibull.cpp

#include <numkit/stats/distributions/weibull.hpp>

#include <numkit/builtin/datafun.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "weibull_detail.hpp"

namespace numkit::stats {


Value wblpdf(const Value &x, double a, double b, std::pmr::memory_resource *mr)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    if (!(a > 0.0) || !(b > 0.0))  // also catches NaN params
        return elementwise(x, [NaN](double){ return NaN; }, mr);
    return elementwise(x, [=](double xi) {
        if (std::isnan(xi)) return NaN;       // propagate NaN x as quiet NaN
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) {
            if (b < 1.0) return std::numeric_limits<double>::infinity();
            if (b > 1.0) return 0.0;
            return 1.0 / a; // b == 1 (exponential)
        }
        const double r = xi / a;
        return (b / a) * std::pow(r, b - 1.0) * std::exp(-std::pow(r, b));
    }, mr);
}

Value wblcdf(const Value &x, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(x, [=](double xi) {
        if (xi <= 0.0) return 0.0;
        return -std::expm1(-std::pow(xi / a, b));
    }, mr);
}

Value wblinv(const Value &p, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi) {
        if (pi < 0.0 || pi > 1.0) return std::numeric_limits<double>::quiet_NaN();
        if (pi == 0.0) return 0.0;
        if (pi >= 1.0) return std::numeric_limits<double>::infinity();
        return a * std::pow(-std::log1p(-pi), 1.0 / b);
    }, mr);
}

Value wblrnd(::numkit::ops::RngContext &rng, double a, double b, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = rng;
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (a <= 0.0 || b <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // std::weibull_distribution(shape=a, scale=b) — note ORDER FLIP relative
    // to MATLAB (a=scale, b=shape).
    std::weibull_distribution<double> wd(b, a);
    for (size_t i = 0; i < n; ++i) od[i] = wd(gen);
    return out;
}

std::tuple<double, double> wblstat(double a, double b)
{
    if (a <= 0.0 || b <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double g1 = std::tgamma(1.0 + 1.0 / b);
    const double g2 = std::tgamma(1.0 + 2.0 / b);
    const double mean = a * g1;
    const double var  = a * a * (g2 - g1 * g1);
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
