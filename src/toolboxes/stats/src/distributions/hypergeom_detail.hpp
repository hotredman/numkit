// toolboxes/stats/src/distributions/hypergeom_detail.hpp
//
// Private (src-only) compute substrate for the hypergeom distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in hypergeom.cpp and
// its CallContext register half in hypergeom_reg.cpp. Kept in an anonymous
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

inline double log_choose(double a, double b) {
    if (b < 0.0 || b > a) return -std::numeric_limits<double>::infinity();
    return std::lgamma(a + 1.0) - std::lgamma(b + 1.0) - std::lgamma(a - b + 1.0);
}

inline bool params_valid(double M, double K, double N) {
    return M >= 0.0 && K >= 0.0 && N >= 0.0
        && std::floor(M) == M && std::floor(K) == K && std::floor(N) == N
        && K <= M && N <= M;
}

inline double hyge_pmf(double k, double M, double K, double N) {
    const double k_min = std::max(0.0, N - (M - K));
    const double k_max = std::min(N, K);
    if (k < k_min || k > k_max || std::floor(k) != k) return 0.0;
    const double lp = log_choose(K, k) + log_choose(M - K, N - k) - log_choose(M, N);
    return std::exp(lp);
}

// Forward sum from k_min, with one-ULP tolerance.
inline double hyge_cdf_scalar(double k, double M, double K, double N) {
    if (k < 0.0) return 0.0;
    const double k_max = std::min(N, K);
    if (k >= k_max) return 1.0;
    const double k_min = std::max(0.0, N - (M - K));
    if (k < k_min) return 0.0;
    // pmf(j+1) / pmf(j) = (K - j)/(j + 1) · (N - j)/(M - K - N + j + 1)
    double f = hyge_pmf(k_min, M, K, N);
    double s = f;
    const double kf = std::floor(k);
    for (double j = k_min; j < kf; j += 1.0) {
        const double num = (K - j) * (N - j);
        const double den = (j + 1.0) * (M - K - N + j + 1.0);
        if (den == 0.0) { f = 0.0; }
        else            { f *= num / den; }
        s += f;
    }
    return std::min(1.0, std::max(0.0, s));
}

inline double hyge_inv_scalar(double q, double M, double K, double N) {
    if (!(q >= 0.0 && q <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    const double k_min = std::max(0.0, N - (M - K));
    const double k_max = std::min(N, K);
    if (q == 0.0) return k_min;
    if (q >= 1.0) return k_max;
    const double tol = std::max(1e-13, q * 1e-13);
    double f = hyge_pmf(k_min, M, K, N);
    double s = f;
    if (s >= q - tol) return k_min;
    for (double j = k_min; j < k_max; j += 1.0) {
        const double num = (K - j) * (N - j);
        const double den = (j + 1.0) * (M - K - N + j + 1.0);
        if (den == 0.0) { f = 0.0; }
        else            { f *= num / den; }
        s += f;
        if (s >= q - tol) return j + 1.0;
    }
    return k_max;
}

// Kernels for the parameter-broadcast path (vector M/K/N). Each wraps
// params_valid + the scalar helper above (all pure double, no mr needed).
inline double hygepdfK(double k, double M, double K, double N) {
    return params_valid(M, K, N) ? hyge_pmf(k, M, K, N)
                                 : std::numeric_limits<double>::quiet_NaN();
}
inline double hygecdfK(double k, double M, double K, double N) {
    return params_valid(M, K, N) ? hyge_cdf_scalar(k, M, K, N)
                                 : std::numeric_limits<double>::quiet_NaN();
}
inline double hygeinvK(double q, double M, double K, double N) {
    return params_valid(M, K, N) ? hyge_inv_scalar(q, M, K, N)
                                 : std::numeric_limits<double>::quiet_NaN();
}

} // anonymous

} // namespace numkit::stats
