// toolboxes/stats/src/distributions/negbin.cpp

#include <numkit/stats/distributions/negbin.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "negbin_detail.hpp"

namespace numkit::stats {


Value nbinpdf(const Value &k, double r, double p, std::pmr::memory_resource *mr)
{
    if (r <= 0.0 || p <= 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return nbin_pmf(ki, r, p); }, mr);
}

Value nbincdf(const Value &k, double r, double p, std::pmr::memory_resource *mr)
{
    if (r <= 0.0 || p <= 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return nbin_cdf_scalar(ki, r, p, mr); }, mr);
}

Value nbininv(const Value &q, double r, double p, std::pmr::memory_resource *mr)
{
    if (!nbin_params_ok(r, p))
        return elementwise(q, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(q, [=](double qi) { return nbin_inv_scalar(qi, r, p); }, mr);
}

Value nbinrnd(double r, double p, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (r <= 0.0 || p <= 0.0 || p > 1.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    // For real (non-integer) r, std::negative_binomial_distribution requires
    // integer k; sample via Gamma-Poisson mixture: λ ~ Gamma(r, (1-p)/p),
    // K | λ ~ Poisson(λ).
    std::gamma_distribution<double> gd(r, (1.0 - p) / p);
    std::poisson_distribution<int>  pd_dummy(1.0); (void)pd_dummy;
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < cnt; ++i) {
        const double lam = gd(gen);
        std::poisson_distribution<int> pd(lam <= 0.0 ? 0.0 : lam);
        od[i] = static_cast<double>(pd(gen));
    }
    return out;
}

std::tuple<double, double> nbinstat(double r, double p)
{
    if (r <= 0.0 || p <= 0.0 || p > 1.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double mean = r * (1.0 - p) / p;
    const double var  = mean / p;
    return std::make_tuple(mean, var);
}

} // namespace numkit::stats
