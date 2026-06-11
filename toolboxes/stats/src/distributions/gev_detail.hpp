// toolboxes/stats/src/distributions/gev_detail.hpp
//
// Private (src-only) compute substrate for the gev distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in gev.cpp and
// its CallContext register half in gev_reg.cpp. Kept in an anonymous
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

constexpr double kEulerGamma = 0.5772156649015328606065120900824024;
constexpr double kPi2Over6   = 1.6449340668482264364724151666460252;

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

// Sample inverse-cdf: x = mu + sigma · ((-log(u))^(-k) - 1)/k for k≠0,
// x = mu − sigma·log(−log(u)) for k=0.
inline double gev_inv_one(double u, double k, double sigma, double mu) {
    if (!(u >= 0.0 && u <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    if (u == 0.0) return (k > 0) ? mu - sigma / k
                                 : -std::numeric_limits<double>::infinity();
    if (u == 1.0) return (k < 0) ? mu - sigma / k
                                 :  std::numeric_limits<double>::infinity();
    if (k == 0.0) return mu - sigma * std::log(-std::log(u));
    return mu + sigma * (std::pow(-std::log(u), -k) - 1.0) / k;
}

} // anonymous

} // namespace numkit::stats
