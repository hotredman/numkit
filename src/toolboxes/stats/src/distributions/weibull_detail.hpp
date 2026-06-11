// toolboxes/stats/src/distributions/weibull_detail.hpp
//
// Private (src-only) compute substrate for the weibull distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in weibull.cpp and
// its CallContext register half in weibull_reg.cpp. Kept in an anonymous
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

// Scalar kernels for the parameter-broadcast path (vector a/b). Own their
// per-element domain (a<=0 or b<=0 → NaN). Mirror the public fns
// bit-identically, including the x∈{0} boundary and NaN propagation.
inline double wblpdfK(double x, double a, double b)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    if (!(a > 0.0) || !(b > 0.0)) return NaN;   // also catches NaN params
    if (std::isnan(x)) return NaN;
    if (x < 0.0) return 0.0;
    if (x == 0.0) {
        if (b < 1.0) return std::numeric_limits<double>::infinity();
        if (b > 1.0) return 0.0;
        return 1.0 / a;   // b == 1 (exponential)
    }
    const double r = x / a;
    return (b / a) * std::pow(r, b - 1.0) * std::exp(-std::pow(r, b));
}

inline double wblcdfK(double x, double a, double b)
{
    if (a <= 0.0 || b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (x <= 0.0) return 0.0;
    return -std::expm1(-std::pow(x / a, b));
}

inline double wblinvK(double p, double a, double b)
{
    if (a <= 0.0 || b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (p < 0.0 || p > 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (p == 0.0) return 0.0;
    if (p >= 1.0) return std::numeric_limits<double>::infinity();
    return a * std::pow(-std::log1p(-p), 1.0 / b);
}

} // anonymous

} // namespace numkit::stats
