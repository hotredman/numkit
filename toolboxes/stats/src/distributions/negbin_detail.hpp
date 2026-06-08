// toolboxes/stats/src/distributions/negbin_detail.hpp
//
// Private (src-only) compute substrate for the negbin distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in negbin.cpp and
// its CallContext register half in negbin_reg.cpp. Kept in an anonymous
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

// log f(k; r, p) = lgamma(k + r) - lgamma(k+1) - lgamma(r)
//                + r·log(p) + k·log(1-p)
inline double nbin_pmf(double k, double r, double p) {
    if (k < 0.0 || std::floor(k) != k) return 0.0;
    if (p == 1.0) return (k == 0.0) ? 1.0 : 0.0;
    const double lc = std::lgamma(k + r) - std::lgamma(k + 1.0) - std::lgamma(r);
    return std::exp(lc + r * std::log(p) + k * std::log1p(-p));
}

inline double nbin_cdf_scalar(double k, double r, double p, std::pmr::memory_resource *mr) {
    if (k < 0.0) return 0.0;
    if (p == 1.0) return 1.0;
    const double kf = std::floor(k);
    // F(k; r, p) = I_p(r, ⌊k⌋ + 1)
    Value xv = Value::scalar(p, mr);
    Value av = Value::scalar(r, mr);
    Value bv = Value::scalar(kf + 1.0, mr);
    Value rv = ::numkit::builtin::betainc(xv, av, bv, mr);
    return rv.toScalar();
}

// (r, p) valid iff r>0 (r may be non-integer) and p ∈ (0, 1]. Else → NaN.
inline bool nbin_params_ok(double r, double p) {
    return r > 0.0 && p > 0.0 && p <= 1.0;
}

inline double nbinpdfK(double k, double r, double p) {
    if (!nbin_params_ok(r, p)) return std::numeric_limits<double>::quiet_NaN();
    return nbin_pmf(k, r, p);
}

// Smallest integer k with F(k; r, p) ≥ qival, via pmf-recurrence
// pmf(k+1)/pmf(k) = (k+r)/(k+1)·(1-p). (r, p) assumed valid.
inline double nbin_inv_scalar(double qival, double r, double p) {
    if (!(qival >= 0.0 && qival <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    if (qival == 0.0) return 0.0;
    if (qival >= 1.0) return std::numeric_limits<double>::infinity();
    if (p == 1.0) return 0.0;
    const double q1m = 1.0 - p;
    double pmf = std::pow(p, r);   // pmf(0)
    double cdf = pmf;
    const double tol = std::max(1e-13, qival * 1e-13);
    if (cdf >= qival - tol) return 0.0;
    for (double k = 0.0; k < 1e12; k += 1.0) {
        pmf *= (k + r) / (k + 1.0) * q1m;
        cdf += pmf;
        if (cdf >= qival - tol) return k + 1.0;
    }
    return std::numeric_limits<double>::infinity();
}

} // anonymous

} // namespace numkit::stats
