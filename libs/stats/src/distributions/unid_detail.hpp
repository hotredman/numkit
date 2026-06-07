// libs/stats/src/distributions/unid_detail.hpp
//
// Private (src-only) compute substrate for the unid distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in unid.cpp and
// its CallContext register half in unid_reg.cpp. Kept in an anonymous
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

// Scalar kernels for the parameter-broadcast path (vector N). Own their
// per-element domain (N<1 or noninteger N → NaN). Mirror the public fns.
inline double unidpdfK(double k, double N) {
    if (N < 1.0 || std::floor(N) != N) return std::numeric_limits<double>::quiet_NaN();
    if (k < 1.0 || k > N || std::floor(k) != k) return 0.0;
    return 1.0 / N;
}

inline double unidcdfK(double k, double N) {
    if (N < 1.0 || std::floor(N) != N) return std::numeric_limits<double>::quiet_NaN();
    if (k < 1.0) return 0.0;
    if (k >= N) return 1.0;
    return std::floor(k) / N;
}

inline double unidinvK(double p, double N) {
    if (!(N >= 1.0) || std::floor(N) != N) return std::numeric_limits<double>::quiet_NaN();
    if (std::isnan(p) || p <= 0.0 || p > 1.0) return std::numeric_limits<double>::quiet_NaN();
    const double r = std::ceil(p * N);
    return r < 1.0 ? 1.0 : (r > N ? N : r);
}

} // anonymous

} // namespace numkit::stats
