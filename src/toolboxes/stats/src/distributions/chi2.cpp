// toolboxes/stats/src/distributions/chi2.cpp
//
// Chi-squared distribution. Composes the existing gammainc /
// gammaincinv on (k/2, x/2) for cdf / icdf.

#include <numkit/stats/distributions/chi2.hpp>

#include <numkit/builtin/datafun.hpp>  // RngContext + rand/randn/randi/randperm (session-scoped, no global/mutex)
#include <numkit/builtin/specfun.hpp> // gammainc, gammaincinv

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <cstring>
#include <mutex>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "chi2_detail.hpp"

namespace numkit::stats {


Value chi2pdf(const Value &x, double k, std::pmr::memory_resource *mr)
{
    // MATLAB convention: k < 0 ⇒ NaN; k == 0 ⇒ 0 (degenerate Chi²(0)
    // has all mass at 0, so density is 0 almost everywhere).
    if (k < 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    if (k == 0.0)
        return elementwise(x, [](double){ return 0.0; }, mr);
    // pdf(x; k) = (1/(2^(k/2) Γ(k/2))) x^(k/2-1) e^(-x/2), x ≥ 0
    // Compute log-pdf and exp to avoid overflow on small/large k.
    const double half_k = 0.5 * k;
    const double log_norm = -half_k * std::log(2.0) - std::lgamma(half_k);
    return elementwise(x, [=](double xi) {
        if (xi < 0.0) return 0.0;
        if (xi == 0.0) return (k == 2.0) ? 0.5 : (k > 2.0 ? 0.0 : std::numeric_limits<double>::infinity());
        const double lp = log_norm + (half_k - 1.0) * std::log(xi) - 0.5 * xi;
        return std::exp(lp);
    }, mr);
}

Value chi2cdf(const Value &x, double k, std::pmr::memory_resource *mr)
{
    if (k <= 0.0)
        return elementwise(x, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    // F(x; k) = gammainc(x/2, k/2) (regularized lower).
    // numkit::builtin::gammainc takes Values for x and a — call elementwise.
    auto out = elementwise(x, [](double xi){ return std::max(0.0, 0.5 * xi); }, mr);
    Value ar = Value::scalar(0.5 * k, mr);
    return ::numkit::builtin::gammainc(out, ar, mr);
}

Value chi2inv(const Value &p, double k, std::pmr::memory_resource *mr)
{
    // MATLAB convention: k < 0 ⇒ NaN; k == 0 ⇒ degenerate, quantile is 0
    // for any p in [0, 1] (out-of-range p still NaN).
    if (k < 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    if (k == 0.0)
        return elementwise(p, [](double pi){
            return (pi >= 0.0 && pi <= 1.0) ? 0.0
                                            : std::numeric_limits<double>::quiet_NaN();
        }, mr);
    Value ar = Value::scalar(0.5 * k, mr);
    Value q = ::numkit::builtin::gammaincinv(p, ar, mr);
    // x = 2 * gammaincinv(p, k/2)
    return elementwise(q, [](double v){ return 2.0 * v; }, mr);
}

Value chi2rnd(::numkit::ops::RngContext &rng, double k, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = rng;
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (k <= 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    // Sample from Gamma(k/2, 2) — std::gamma_distribution(shape, scale).
    std::gamma_distribution<double> gd(0.5 * k, 2.0);
    for (size_t i = 0; i < n; ++i) od[i] = gd(gen);
    return out;
}

std::tuple<double, double> chi2stat(double k)
{
    // MATLAB convention: k <= 0 ⇒ moments NaN (degenerate distribution).
    if (k <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(k, 2.0 * k);
}

} // namespace numkit::stats
