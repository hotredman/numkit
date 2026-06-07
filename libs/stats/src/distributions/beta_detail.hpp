// libs/stats/src/distributions/beta_detail.hpp
//
// Private (src-only) compute substrate for the beta distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in beta.cpp and
// its CallContext register half in beta_reg.cpp. Kept in an anonymous
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

// Scalar pdf kernel for the parameter-broadcast path (vector a/b). Owns its
// per-element domain: a<=0 or b<=0 → NaN. Mirrors public betapdf exactly,
// including the x∈{0,1} boundary cases.
inline double betapdfK(double x, double a, double b)
{
    if (a <= 0.0 || b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    if (x < 0.0 || x > 1.0) return 0.0;
    if (x == 0.0)
        return (a == 1.0) ? std::exp(-lbeta) * (b == 1.0 ? 1.0 : std::pow(1.0, b - 1.0))
                          : (a > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
    if (x == 1.0)
        return (b == 1.0) ? std::exp(-lbeta) * (a == 1.0 ? 1.0 : std::pow(1.0, a - 1.0))
                          : (b > 1.0 ? 0.0 : std::numeric_limits<double>::infinity());
    return std::exp((a - 1.0) * std::log(x) + (b - 1.0) * std::log1p(-x) - lbeta);
}

} // anonymous

} // namespace numkit::stats
