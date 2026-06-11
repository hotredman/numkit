// toolboxes/stats/src/distributions/exponential_detail.hpp
//
// Private (src-only) compute substrate for the exponential distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in exponential.cpp and
// its CallContext register half in exponential_reg.cpp. Kept in an anonymous
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

// ── Scalar kernels (single source of truth) ──────────────────────────
// Each owns its per-element domain handling so they can be broadcast over a
// vector mu: mu<=0 (or NaN) → NaN, matching MATLAB R2025b.

inline double exppdfK(double x, double mu)
{
    if (!(mu > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    if (x < 0.0) return 0.0;
    const double inv_mu = 1.0 / mu;
    return inv_mu * std::exp(-x * inv_mu);
}

inline double expcdfK(double x, double mu)
{
    if (!(mu > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    if (x <= 0.0) return 0.0;
    const double inv_mu = 1.0 / mu;
    return -std::expm1(-x * inv_mu);
}

inline double expinvK(double p, double mu)
{
    if (!(mu > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    if (p < 0.0 || p > 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (p >= 1.0) return std::numeric_limits<double>::infinity();
    return -mu * std::log1p(-p);
}

} // anonymous

} // namespace numkit::stats
