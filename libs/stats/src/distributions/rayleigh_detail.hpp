// libs/stats/src/distributions/rayleigh_detail.hpp
//
// Private (src-only) compute substrate for the rayleigh distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in rayleigh.cpp and
// its CallContext register half in rayleigh_reg.cpp. Kept in an anonymous
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

// Scalar kernels for the parameter-broadcast path (vector b). Own their
// per-element domain (b<=0 → NaN). Mirror the public fns bit-identically.
inline double raylpdfK(double x, double b)
{
    if (b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double inv_b2 = 1.0 / (b * b);
    if (x < 0.0) return 0.0;
    return x * inv_b2 * std::exp(-0.5 * x * x * inv_b2);
}

inline double raylcdfK(double x, double b)
{
    if (b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double inv_b2 = 1.0 / (b * b);
    if (x <= 0.0) return 0.0;
    return -std::expm1(-0.5 * x * x * inv_b2);
}

inline double raylinvK(double p, double b)
{
    if (b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (p < 0.0 || p > 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return 0.0;
    if (p >= 1.0) return std::numeric_limits<double>::infinity();
    return b * std::sqrt(-2.0 * std::log1p(-p));
}

} // anonymous

} // namespace numkit::stats
