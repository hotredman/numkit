// toolboxes/stats/src/distributions/chi2_detail.hpp
//
// Private (src-only) compute substrate for the chi2 distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in chi2.cpp and
// its CallContext register half in chi2_reg.cpp. Kept in an anonymous
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

// Scalar pdf kernel for the parameter-broadcast path (vector k). Owns its
// per-element domain: k<0 → NaN, k==0 → 0 (degenerate Chi²(0)). Mirrors the
// public chi2pdf formula exactly.
inline double chi2pdfK(double x, double k)
{
    if (k < 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (k == 0.0) return 0.0;
    if (x < 0.0) return 0.0;
    const double half_k = 0.5 * k;
    const double log_norm = -half_k * std::log(2.0) - std::lgamma(half_k);
    if (x == 0.0)
        return (k == 2.0) ? 0.5 : (k > 2.0 ? 0.0 : std::numeric_limits<double>::infinity());
    return std::exp(log_norm + (half_k - 1.0) * std::log(x) - 0.5 * x);
}

} // anonymous

} // namespace numkit::stats
