// toolboxes/stats/src/distributions/ncx2_detail.hpp
//
// Private (src-only) compute substrate for the ncx2 distribution:
// the scalar *K kernels + elementwise template + local helpers, shared
// between the engine-free compute (public *pdf/*cdf/*inv) in ncx2.cpp and
// its CallContext register half in ncx2_reg.cpp. Kept in an anonymous
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

inline double besseli_scalar(double nu, double xx, std::pmr::memory_resource *mr)
{
    Value nv = Value::scalar(nu, mr);
    Value xv = Value::scalar(xx, mr);
    return ::numkit::math::besseli(nv, xv, mr).toScalar();
}

inline double gammainc_scalar(double xx, double a, std::pmr::memory_resource *mr)
{
    Value xv = Value::scalar(xx, mr);
    Value av = Value::scalar(a,  mr);
    return ::numkit::math::gammainc(xv, av, mr).toScalar();
}

double ncx2cdf_one(double x, double k, double lambda, std::pmr::memory_resource *mr)
{
    if (x <= 0.0) return 0.0;
    if (lambda == 0.0) return gammainc_scalar(x / 2.0, k / 2.0, mr);
    const double halfL = lambda / 2.0;
    const double halfX = x / 2.0;
    double Pj = std::exp(-halfL);
    double sum = 0.0;
    const int maxIter = 2000;
    for (int j = 0; j < maxIter; ++j) {
        const double cdfTerm = gammainc_scalar(halfX, k / 2.0 + double(j), mr);
        const double contrib = Pj * cdfTerm;
        sum += contrib;
        if (j > 5 && contrib < 1e-16 * (sum + 1e-300)) break;
        Pj *= halfL / double(j + 1);
    }
    return std::min(1.0, sum);
}

} // anonymous

} // namespace numkit::stats
