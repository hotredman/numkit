// libs/stats/src/distributions/gamma_dist_detail.hpp
//
// Private (src-only) compute substrate for the gamma_dist distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in gamma_dist.cpp and
// its CallContext register half in gamma_dist_reg.cpp. Kept in an anonymous
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
// per-element domain: a<0 or b<=0 → NaN, a==0 → 0. Mirrors public gampdf.
inline double gampdfK(double x, double a, double b)
{
    if (a < 0.0 || b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (a == 0.0) return 0.0;
    if (x < 0.0) return 0.0;
    const double log_b = std::log(b);
    const double lga   = std::lgamma(a);
    if (x == 0.0) {
        if (a < 1.0) return std::numeric_limits<double>::infinity();
        if (a > 1.0) return 0.0;
        return std::exp(-lga - log_b);   // a == 1 → 1/b
    }
    return std::exp((a - 1.0) * std::log(x) - x / b - a * log_b - lga);
}

} // anonymous

} // namespace numkit::stats
