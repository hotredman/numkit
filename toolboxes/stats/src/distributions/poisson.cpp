// toolboxes/stats/src/distributions/poisson.cpp

#include <numkit/stats/distributions/poisson.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

#include "poisson_detail.hpp"

namespace numkit::stats {


Value poisspdf(const Value &k, double lambda, std::pmr::memory_resource *mr)
{
    if (lambda < 0.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return poiss_pmf(ki, lambda); }, mr);
}

Value poisscdf(const Value &k, double lambda, std::pmr::memory_resource *mr)
{
    if (lambda < 0.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    if (lambda == 0.0)
        return elementwise(k, [](double ki){ return ki >= 0.0 ? 1.0 : 0.0; }, mr);
    // Build a vector x = floor(k) + 1, run gammainc(λ, x), then F = 1 - that.
    Value xs = elementwise(k, [=](double ki) {
        if (ki < 0.0) return 0.0;        // clamp; gammainc(λ, 0) sentinel
        return std::floor(ki) + 1.0;
    }, mr);
    Value lam = Value::scalar(lambda, mr);
    Value lower = ::numkit::math::gammainc(lam, xs, mr);
    // F = 1 - P, but: for ki < 0, xs = 0 and we want F = 0. gammainc(λ, 0) is
    // technically undefined; if it returns 0, F = 1 — wrong. Patch via a
    // walk that knows the original ki.
    Value out;
    const auto &d = k.dims();
    if (k.isScalar()) {
        const double ki = k.toScalar();
        if (ki < 0.0) return Value::scalar(0.0, mr);
        return Value::scalar(1.0 - lower.elemAsDouble(0), mr);
    }
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    else          out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    const size_t n = k.numel();
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const double ki = k.elemAsDouble(i);
        if (ki < 0.0) { od[i] = 0.0; continue; }
        od[i] = 1.0 - lower.elemAsDouble(i);
    }
    return out;
}


Value poissinv(const Value &p, double lambda, std::pmr::memory_resource *mr)
{
    if (lambda < 0.0)
        return elementwise(p, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p, [=](double pi){ return poiss_inv_scalar(pi, lambda); }, mr);
}

Value poissrnd(double lambda, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (lambda < 0.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t n = rows * cols;
    if (lambda == 0.0) {
        for (size_t i = 0; i < n; ++i) od[i] = 0.0;
        return out;
    }
    std::poisson_distribution<int> pd(lambda);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < n; ++i) od[i] = static_cast<double>(pd(gen));
    return out;
}

std::tuple<double, double> poisstat(double lambda)
{
    // MATLAB convention: lambda <= 0 ⇒ NaN/NaN (degenerate at 0).
    if (lambda <= 0.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(lambda, lambda);
}

} // namespace numkit::stats
