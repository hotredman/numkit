// toolboxes/stats/src/distributions/students_t_detail.hpp
//
// Private (src-only) compute substrate for the students_t distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in students_t.cpp and
// its CallContext register half in students_t_reg.cpp. Kept in an anonymous
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

// Read a Value's first element as scalar (for inputs we know are
// scalar Values produced by the helpers). Avoids extra copies.
double scalarOf(const Value &v) { return v.toScalar(); }

// ── Central-t scalar helpers for the parameter-broadcast path (vector nu) ──
// tpdf is closed-form; tcdf/tinv compose betainc/betaincinv so they use a
// per-element fixup loop in the adapter, but the nu→∞ Gaussian limit (which
// the public scalar fns special-case) is handled per element via these.
inline double tpdfK(double x, double nu)
{
    if (!(nu > 0.0)) return std::numeric_limits<double>::quiet_NaN();   // nu<=0 / NaN
    if (std::isinf(nu)) {
        const double inv = 1.0 / std::sqrt(2.0 * M_PI);
        return inv * std::exp(-0.5 * x * x);
    }
    const double log_norm = std::lgamma(0.5 * (nu + 1.0))
                          - std::lgamma(0.5 * nu)
                          - 0.5 * std::log(nu * M_PI);
    return std::exp(log_norm - 0.5 * (nu + 1.0) * std::log1p(x * x / nu));
}

inline double tNormCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

// norminv via Winitzki erfinv + 2 Newton steps (same construction as the
// public tinv's nu==Inf branch).
inline double tNormInv(double p)
{
    const double y = 2.0 * p - 1.0;
    if (y >= 1.0) return std::numeric_limits<double>::infinity();
    if (y <= -1.0) return -std::numeric_limits<double>::infinity();
    const double a = 0.147;
    const double ln1m = std::log(1.0 - y * y);
    const double term = 2.0 / (M_PI * a) + 0.5 * ln1m;
    double xv = std::copysign(std::sqrt(std::sqrt(term * term - ln1m / a) - term), y);
    for (int it = 0; it < 2; ++it) {
        const double e  = std::erf(xv) - y;
        const double de = 2.0 * std::exp(-xv * xv) / std::sqrt(M_PI);
        xv -= e / de;
    }
    return std::sqrt(2.0) * xv;
}

} // anonymous
namespace {

// Standard normal CDF.
inline double phiCdf(double z)
{
    return 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
}

inline double betainc_scalar(double y, double a, double b, std::pmr::memory_resource *mr)
{
    Value yv = Value::scalar(y, mr);
    Value av = Value::scalar(a, mr);
    Value bv = Value::scalar(b, mr);
    return ::numkit::math::betainc(yv, av, bv, mr).toScalar();
}

double nctpdf_one(double x, double nu, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu > 0.0))
        return std::numeric_limits<double>::quiet_NaN();
    // δ = 0 → central tpdf.
    if (delta == 0.0) {
        const double log_norm = std::lgamma(0.5 * (nu + 1.0))
                              - std::lgamma(0.5 * nu)
                              - 0.5 * std::log(nu * M_PI);
        return std::exp(log_norm - 0.5 * (nu + 1.0) * std::log1p(x * x / nu));
    }
    if (x == 0.0) {
        // f(0; ν, δ) = Γ((ν+1)/2) / [√(νπ) · Γ(ν/2)] · exp(-δ²/2)
        const double log_v = std::lgamma(0.5 * (nu + 1.0))
                           - 0.5 * std::log(nu * M_PI)
                           - std::lgamma(0.5 * nu)
                           - 0.5 * delta * delta;
        return std::exp(log_v);
    }
    const double npx2 = nu + x * x;
    // log-prefactor: (ν/2)·log(ν) - δ²/2 - 0.5·log(π) - lgamma(ν/2)
    //              - (ν+1)/2 · log(ν + x²)
    const double log_pref = 0.5 * nu * std::log(nu)
                          - 0.5 * delta * delta
                          - 0.5 * std::log(M_PI)
                          - std::lgamma(0.5 * nu)
                          - 0.5 * (nu + 1.0) * std::log(npx2);
    const double xd = x * delta;
    const int sign_factor = (xd >= 0.0) ? 1 : -1;
    const double log_t = std::log(std::fabs(xd) * std::sqrt(2.0));   // log(|xδ|√2)
    const double half_log_npx2 = 0.5 * std::log(npx2);

    double sum = 0.0, abs_sum = 0.0;
    int sign_k = 1;
    constexpr int kMax = 2000;
    for (int k = 0; k < kMax; ++k) {
        const double log_termk = std::lgamma(0.5 * (nu + double(k) + 1.0))
                              + double(k) * log_t
                              - double(k) * half_log_npx2
                              - std::lgamma(double(k) + 1.0);
        const double abs_term = std::exp(log_termk);
        const double contrib = sign_k * abs_term;
        sum += contrib;
        abs_sum += abs_term;
        if (k > 10 && abs_term < 1e-16 * abs_sum) break;
        sign_k *= sign_factor;
    }
    double v = std::exp(log_pref) * sum;
    if (!std::isfinite(v) || v < 0.0) v = std::max(v, 0.0);
    return v;
}

double nctcdf_one(double x, double nu, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu > 0.0))
        return std::numeric_limits<double>::quiet_NaN();
    if (delta == 0.0) {
        // Central tcdf branch.
        const double z = nu / (nu + x * x);
        const double I = betainc_scalar(z, 0.5 * nu, 0.5, mr);
        return (x >= 0.0) ? 1.0 - 0.5 * I : 0.5 * I;
    }
    // Symmetry for negative x.
    if (x < 0.0) return 1.0 - nctcdf_one(-x, nu, -delta, mr);
    // At x = 0: F = Φ(-δ).
    if (x == 0.0) return phiCdf(-delta);

    const double y = (x * x) / (x * x + nu);
    const double z = 0.5 * delta * delta;
    const double phi_neg_d = phiCdf(-delta);

    // Series 1 (P_k coefficient): Poisson pmf with mean z.
    // Series 2 (Q_k coefficient): (δ/(2√(2π))) · (z^k / Γ(k+3/2)) · e^{-z}.
    const double e_neg_z = std::exp(-z);
    double alpha = e_neg_z;                                    // P_0
    double beta  = (2.0 / std::sqrt(M_PI)) * e_neg_z;          // (z^0 / Γ(3/2)) · e^{-z}

    double sum1 = 0.0, sum2 = 0.0;
    constexpr int kMax = 2000;
    for (int k = 0; k < kMax; ++k) {
        const double I1 = betainc_scalar(y, double(k) + 0.5, 0.5 * nu, mr);
        const double I2 = betainc_scalar(y, double(k) + 1.0, 0.5 * nu, mr);
        const double t1 = alpha * I1;
        const double t2 = beta  * I2;
        sum1 += t1;
        sum2 += t2;
        if (k > 10 && t1 < 1e-16 * (sum1 + 1e-300)
                  && t2 < 1e-16 * (sum2 + 1e-300)) break;
        // Recurrences.
        alpha *= z / double(k + 1);
        beta  *= z / (double(k) + 1.5);
    }

    double F = phi_neg_d + 0.5 * sum1 + (delta / (2.0 * std::sqrt(2.0))) * sum2;
    if (F < 0.0) F = 0.0;
    if (F > 1.0) F = 1.0;
    return F;
}

} // anonymous
namespace {

// Central tinv (scalar): inverse of central t-cdf via betaincinv.
double tinv_scalar(double p, double nu, std::pmr::memory_resource *mr)
{
    if (p <= 0.0) return -std::numeric_limits<double>::infinity();
    if (p >= 1.0) return  std::numeric_limits<double>::infinity();
    if (p == 0.5) return 0.0;
    // betaincinv(2·min(p,1-p), ν/2, ½) → y; then x = sign·sqrt(ν(1/y - 1)).
    const bool lower_half = (p < 0.5);
    const double tail = lower_half ? 2.0 * p : 2.0 * (1.0 - p);
    Value tv = Value::scalar(tail, mr);
    Value av = Value::scalar(0.5 * nu, mr);
    Value bv = Value::scalar(0.5, mr);
    const double y = ::numkit::math::betaincinv(tv, av, bv, mr).toScalar();
    if (y <= 0.0) return lower_half ? -std::numeric_limits<double>::infinity()
                                     :  std::numeric_limits<double>::infinity();
    if (y >= 1.0) return 0.0;
    const double x = std::sqrt(nu * (1.0 / y - 1.0));
    return lower_half ? -x : x;
}

double nctinv_one(double p, double nu, double delta,
                  std::pmr::memory_resource *mr)
{
    if (!(nu > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    if (std::isnan(p) || p < 0.0 || p > 1.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return -std::numeric_limits<double>::infinity();
    if (p == 1.0) return  std::numeric_limits<double>::infinity();
    if (delta == 0.0) return tinv_scalar(p, nu, mr);

    // Initial guess: central tinv shifted by δ.
    double x = tinv_scalar(p, nu, mr) + delta;
    if (!std::isfinite(x)) x = delta;

    // Establish a bracket [lo, hi] for fallback bisection.
    // Find any x_lo with cdf < p and any x_hi with cdf > p.
    double lo = std::min(x - 1.0, delta - 50.0);
    double hi = std::max(x + 1.0, delta + 50.0);
    while (nctcdf_one(lo, nu, delta, mr) > p) {
        lo -= std::max(1.0, std::fabs(lo));
        if (!std::isfinite(lo)) break;
    }
    while (nctcdf_one(hi, nu, delta, mr) < p) {
        hi += std::max(1.0, std::fabs(hi));
        if (!std::isfinite(hi)) break;
    }

    // Newton + bisection guard, ≤ 60 iterations.
    for (int it = 0; it < 60; ++it) {
        const double F  = nctcdf_one(x, nu, delta, mr);
        const double f  = nctpdf_one(x, nu, delta, mr);
        const double err = F - p;
        if (std::fabs(err) < 1e-14) return x;
        // Bisect endpoint update.
        if (err > 0.0) hi = x; else lo = x;
        double x_new;
        if (f > 1e-300) {
            x_new = x - err / f;
            // Reject Newton step outside bracket — bisect instead.
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
