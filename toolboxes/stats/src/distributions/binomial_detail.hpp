// toolboxes/stats/src/distributions/binomial_detail.hpp
//
// Private (src-only) compute substrate for the binomial distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in binomial.cpp and
// its CallContext register half in binomial_reg.cpp. Kept in an anonymous
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

inline double bino_pmf(double k, double n, double p) {
    if (k < 0.0 || k > n || std::floor(k) != k) return 0.0;
    if (p == 0.0) return (k == 0.0) ? 1.0 : 0.0;
    if (p == 1.0) return (k == n)   ? 1.0 : 0.0;
    // log f = lgamma(n+1) - lgamma(k+1) - lgamma(n-k+1) + k log p + (n-k) log(1-p)
    const double lc = std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
    return std::exp(lc + k * std::log(p) + (n - k) * std::log1p(-p));
}

inline double bino_cdf_scalar(double k, double n, double p, std::pmr::memory_resource *mr) {
    if (k < 0.0) return 0.0;
    if (k >= n) return 1.0;
    if (p == 0.0) return 1.0;            // P(K = 0) = 1
    if (p == 1.0) return 0.0;            // P(K < n) = 0
    const double kf = std::floor(k);
    // F(k) = I_{1-p}(n - kf, kf + 1)
    Value xv = Value::scalar(1.0 - p, mr);
    Value av = Value::scalar(n - kf, mr);
    Value bv = Value::scalar(kf + 1.0, mr);
    Value r = ::numkit::math::betainc(xv, av, bv, mr);
    return r.toScalar();
}

// True iff (n, p) is a valid binomial parameter pair (n a nonneg integer,
// p ∈ [0, 1]). Out-of-domain → the whole result is NaN, per element.
inline bool bino_params_ok(double n, double p) {
    return n >= 0.0 && std::floor(n) == n && p >= 0.0 && p <= 1.0;
}

inline double binopdfK(double k, double n, double p) {
    if (!bino_params_ok(n, p)) return std::numeric_limits<double>::quiet_NaN();
    return bino_pmf(k, n, p);
}

// Smallest integer j with F(j; n, p) ≥ pival (discrete quantile), via the pmf
// recurrence pmf(j+1)/pmf(j) = (n-j)/(j+1)·p/(1-p). (n, p) assumed valid.
inline double bino_inv_scalar(double pival, double n, double p) {
    if (!(pival >= 0.0 && pival <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    if (pival == 0.0) return 0.0;
    if (pival >= 1.0) return n;
    if (n == 0.0) return 0.0;
    const double r = p / (1.0 - p);
    double pmf = std::pow(1.0 - p, n);  // pmf(0)
    double cdf = pmf;
    const double tol = std::max(1e-13, pival * 1e-13);
    if (cdf >= pival - tol) return 0.0;
    for (double j = 0.0; j < n; j += 1.0) {
        pmf *= (n - j) / (j + 1.0) * r;
        cdf += pmf;
        if (cdf >= pival - tol) return j + 1.0;
    }
    return n;
}

} // anonymous

} // namespace numkit::stats
