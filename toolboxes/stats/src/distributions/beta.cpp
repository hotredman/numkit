// toolboxes/stats/src/distributions/beta.cpp
//
// Beta distribution. pdf via log-form using lbeta = lgamma(a)+lgamma(b)-lgamma(a+b);
// cdf is betainc(x, a, b); icdf is betaincinv(p, a, b); rnd via two Gamma draws.

#include <numkit/stats/distributions/beta.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "beta_detail.hpp"

namespace numkit::stats {


Value betapdf(const Value &x, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // log f(x) = (a-1) log x + (b-1) log(1-x) - lbeta(a, b)
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    return elementwise(x, [=](double xi) {
        if (xi < 0.0 || xi > 1.0) return 0.0;
        if (xi == 0.0) return (a == 1.0) ? std::exp(-lbeta) * (b == 1.0 ? 1.0 : std::pow(1.0, b - 1.0))
                                          : (a > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
        if (xi == 1.0) return (b == 1.0) ? std::exp(-lbeta) * (a == 1.0 ? 1.0 : std::pow(1.0, a - 1.0))
                                          : (b > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
        const double lp = (a - 1.0) * std::log(xi)
                        + (b - 1.0) * std::log1p(-xi)
                        - lbeta;
        return std::exp(lp);
    }, mr);
}

Value betacdf(const Value &x, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    // Clamp into [0, 1] so betainc behaves on out-of-domain input.
    Value xc = elementwise(x, [](double xi) {
        if (xi <= 0.0) return 0.0;
        if (xi >= 1.0) return 1.0;
        return xi;
    }, mr);
    return ::numkit::builtin::betainc(xc, av, bv, mr);
}

Value betainv(const Value &p, double a, double b, std::pmr::memory_resource *mr)
{
    if (a <= 0.0 || b <= 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    return ::numkit::builtin::betaincinv(p, av, bv, mr);
}

Value betarnd(double a, double b, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (a <= 0.0 || b <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    std::gamma_distribution<double> ga(a, 1.0);
    std::gamma_distribution<double> gb(b, 1.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) {
        const double u = ga(gen);
        const double v = gb(gen);
        const double s = u + v;
        od[i] = (s > 0.0) ? u / s : 0.5;
    }
    return out;
}

std::tuple<double, double> betastat(double a, double b)
{
    if (a <= 0.0 || b <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double s = a + b;
    const double mean = a / s;
    const double var = (a * b) / (s * s * (s + 1.0));
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
