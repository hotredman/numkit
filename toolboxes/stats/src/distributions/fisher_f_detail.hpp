// toolboxes/stats/src/distributions/fisher_f_detail.hpp
//
// Private (src-only) compute substrate for the fisher_f distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in fisher_f.cpp and
// its CallContext register half in fisher_f_reg.cpp. Kept in an anonymous
// namespace (internal linkage per TU) — pure stateless math, no ODR risk.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

template <typename Op>
Value elementwise(const Value &x, Op op, std::pmr::memory_resource *mr)
{
    if (x.isScalar()) return Value::scalar(op(x.toScalar()), mr);
    const auto &d = x.dims();
    Value out;
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    else          out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    const size_t n = x.numel();
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) od[i] = op(x.elemAsDouble(i));
    return out;
}

// Scalar pdf kernel for the parameter-broadcast path (vector v1/v2). Owns its
// per-element domain (v1<=0 or v2<=0 → NaN) and the x==0 boundary regimes.
// Mirrors the public fpdf bit-identically.
inline double fpdfK(double x, double v1, double v2)
{
    if (v1 <= 0.0 || v2 <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double a = 0.5 * v1;
    const double b = 0.5 * v2;
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    const double log_v1 = std::log(v1);
    const double log_v2 = std::log(v2);
    if (x < 0.0) return 0.0;
    if (x == 0.0) {
        if (v1 < 2.0) return std::numeric_limits<double>::infinity();
        if (v1 == 2.0)
            return std::exp(a * log_v1 + b * log_v2 - (a + b) * std::log(v2) - lbeta);
        return 0.0;
    }
    return std::exp(a * log_v1 + b * log_v2 + (a - 1.0) * std::log(x)
                    - (a + b) * std::log(v2 + v1 * x) - lbeta);
}

} // anonymous
namespace {

double ncfpdf_one(double x, double nu1, double nu2, double delta)
{
    if (!(nu1 > 0.0) || !(nu2 > 0.0) || delta < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (x <= 0.0) return 0.0;
    if (delta == 0.0) {
        // Central F pdf in log-space.
        const double log_norm = std::lgamma(0.5 * (nu1 + nu2))
                              - std::lgamma(0.5 * nu1)
                              - std::lgamma(0.5 * nu2)
                              + 0.5 * nu1 * (std::log(nu1) - std::log(nu2));
        return std::exp(log_norm + (0.5 * nu1 - 1.0) * std::log(x)
                       - 0.5 * (nu1 + nu2) * std::log1p(nu1 * x / nu2));
    }
    const double L = 0.5 * delta;
    const double log_L = std::log(L);
    const double log_r = std::log(nu1 / nu2);
    const double log_x = std::log(x);
    const double log_one_plus_rx = std::log1p(nu1 * x / nu2);

    double sum = 0.0, abs_sum = 0.0;
    constexpr int kMax = 2000;
    for (int k = 0; k < kMax; ++k) {
        // log term_k
        const double a = 0.5 * nu1 + double(k);
        const double b = 0.5 * nu2;
        const double log_beta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
        const double log_term = -L + double(k) * log_L - std::lgamma(double(k) + 1.0)
                              + a * log_r
                              + (a - 1.0) * log_x
                              - (a + b) * log_one_plus_rx
                              - log_beta;
        const double t = std::exp(log_term);
        sum += t;
        abs_sum += t;
        if (k > 10 && t < 1e-16 * abs_sum) break;
    }
    return std::max(0.0, sum);
}

} // anonymous
namespace {

inline double betainc_scalar(double y, double a, double b,
                             std::pmr::memory_resource *mr)
{
    Value yv = Value::scalar(y, mr);
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    return ::numkit::math::betainc(yv, av, bv, mr).toScalar();
}

double ncfcdf_one(double x, double nu1, double nu2, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu1 > 0.0) || !(nu2 > 0.0) || delta < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (x <= 0.0) return 0.0;
    const double y = (nu1 * x) / (nu1 * x + nu2);
    if (delta == 0.0)
        return betainc_scalar(y, 0.5 * nu1, 0.5 * nu2, mr);

    const double L = 0.5 * delta;
    double Pj = std::exp(-L);
    double sum = 0.0;
    constexpr int kMax = 2000;
    for (int k = 0; k < kMax; ++k) {
        const double I = betainc_scalar(y, 0.5 * nu1 + double(k), 0.5 * nu2, mr);
        const double t = Pj * I;
        sum += t;
        if (k > 5 && t < 1e-16 * (sum + 1e-300)) break;
        Pj *= L / double(k + 1);
    }
    if (sum > 1.0) sum = 1.0;
    if (sum < 0.0) sum = 0.0;
    return sum;
}

double ncfinv_one(double p, double nu1, double nu2, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu1 > 0.0) || !(nu2 > 0.0) || delta < 0.0
        || std::isnan(p) || p < 0.0 || p > 1.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return 0.0;
    if (p == 1.0) return std::numeric_limits<double>::infinity();

    // Initial guess: central finv (or rough fallback for δ > 0).
    double x;
    {
        Value pv = Value::scalar(p, mr);
        x = finv(pv, nu1, nu2, mr).toScalar();
        if (!(x > 0.0) || !std::isfinite(x)) x = 1.0;
    }
    // Heuristic right-shift for δ > 0: noncentral F has larger mean.
    if (delta > 0.0) x *= (1.0 + delta / nu1);

    double lo = 0.0;
    double hi = std::max(x + 1.0, 50.0 * x + 50.0);
    while (ncfcdf_one(hi, nu1, nu2, delta, mr) < p) {
        hi *= 2.0;
        if (!std::isfinite(hi)) break;
    }

    for (int it = 0; it < 80; ++it) {
        const double F  = ncfcdf_one(x, nu1, nu2, delta, mr);
        const double f  = ncfpdf_one(x, nu1, nu2, delta);
        const double err = F - p;
        if (std::fabs(err) < 1e-14) return x;
        if (err > 0.0) hi = x; else lo = x;
        double x_new;
        if (f > 1e-300) {
            x_new = x - err / f;
            if (!std::isfinite(x_new) || x_new <= lo || x_new >= hi)
                x_new = 0.5 * (lo + hi);
        } else {
            x_new = 0.5 * (lo + hi);
        }
        if (std::fabs(x_new - x) < 1e-14 * std::max(1.0, std::fabs(x_new)))
            return x_new;
        x = x_new;
    }
    return x;
}

} // anonymous

} // namespace numkit::stats
