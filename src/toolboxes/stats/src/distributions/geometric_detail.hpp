// toolboxes/stats/src/distributions/geometric_detail.hpp
//
// Private (src-only) compute substrate for the geometric distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in geometric.cpp and
// its CallContext register half in geometric_reg.cpp. Kept in an anonymous
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

// Scalar kernels for the parameter-broadcast path (vector p). Own their
// per-element domain (p<=0 or p>1 → NaN). Mirror the public fns. MATLAB's
// geometric starts at k=0.
inline double geopdfK(double k, double p) {
    if (p <= 0.0 || p > 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (k < 0.0 || std::floor(k) != k) return 0.0;
    if (p == 1.0) return k == 0.0 ? 1.0 : 0.0;
    return std::pow(1.0 - p, k) * p;
}

inline double geocdfK(double k, double p) {
    if (p <= 0.0 || p > 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (k < 0.0) return 0.0;
    if (p == 1.0) return k >= 0.0 ? 1.0 : 0.0;
    return -std::expm1((std::floor(k) + 1.0) * std::log1p(-p));   // 1 - (1-p)^(⌊k⌋+1)
}

inline double geoinvK(double q, double p) {
    if (p <= 0.0 || p > 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (!(q >= 0.0 && q <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    if (q == 0.0) return 0.0;
    if (q >= 1.0) return std::numeric_limits<double>::infinity();
    if (p == 1.0) return 0.0;
    const double v = std::log1p(-q) / std::log1p(-p) - 1.0;
    double k = std::ceil(v);
    if (k < 0.0) k = 0.0;
    if (k > 0.0) {
        const double cdf_prev = -std::expm1(k * std::log1p(-p));   // F(k-1)
        const double tol = std::max(1e-13, q * 1e-13);
        if (cdf_prev >= q - tol) k -= 1.0;
    }
    return k;
}

} // anonymous

} // namespace numkit::stats
