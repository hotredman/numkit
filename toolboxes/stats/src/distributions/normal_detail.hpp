// toolboxes/stats/src/distributions/normal_detail.hpp
//
// Private (src-only) compute substrate for the normal distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in normal.cpp and
// its CallContext register half in normal_reg.cpp. Kept in an anonymous
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

constexpr double kSqrt2  = 1.41421356237309504880;
constexpr double kSqrt2Pi = 2.50662827463100050241;

// Apply f(x[i]) elementwise into a fresh DOUBLE Value of same shape.
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

// MATLAB's erfinv uses the Beasley-Springer-Moro initial + 1 Newton
// step. For our purposes the existing builtin::erfinv is sufficient
// (already tested at ULP-10 in the parity bench).

double erfinvScalar(double y)
{
    // Use the Winitzki approximation as a closed-form fallback if a
    // direct erfinv isn't reachable from the stats lib without a
    // public dep on toolboxes/builtin internals. Accuracy ~1e-3; we then
    // refine with one Newton step on erf.
    if (y >= 1.0) return std::numeric_limits<double>::infinity();
    if (y <= -1.0) return -std::numeric_limits<double>::infinity();
    const double a = 0.147;
    const double ln1m = std::log(1.0 - y * y);
    const double term = 2.0 / (M_PI * a) + 0.5 * ln1m;
    double x = std::copysign(std::sqrt(std::sqrt(term * term - ln1m / a) - term), y);
    // 2 Newton iterations on erf(x) - y for full precision.
    for (int i = 0; i < 2; ++i) {
        const double e = std::erf(x) - y;
        const double de = 2.0 * std::exp(-x * x) / std::sqrt(M_PI);
        x -= e / de;
    }
    return x;
}

// ── Scalar kernels (single source of truth) ──────────────────────────
// Each owns its per-element domain handling so they can be broadcast over
// vector parameters: sigma<=0 (or NaN) → NaN, matching MATLAB R2025b.

inline double normpdfK(double x, double mu, double sigma)
{
    if (!(sigma > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    const double inv = 1.0 / (sigma * kSqrt2Pi);
    const double inv2s2 = 1.0 / (2.0 * sigma * sigma);
    const double d = x - mu;
    return inv * std::exp(-d * d * inv2s2);
}

inline double normcdfK(double x, double mu, double sigma)
{
    if (!(sigma > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    return 0.5 * (1.0 + std::erf((x - mu) / (sigma * kSqrt2)));
}

inline double norminvK(double p, double mu, double sigma)
{
    if (!(sigma > 0.0) || std::isnan(p) || p < 0.0 || p > 1.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return -std::numeric_limits<double>::infinity();
    if (p == 1.0) return  std::numeric_limits<double>::infinity();
    return mu + sigma * kSqrt2 * erfinvScalar(2.0 * p - 1.0);
}

} // anonymous

} // namespace numkit::stats
