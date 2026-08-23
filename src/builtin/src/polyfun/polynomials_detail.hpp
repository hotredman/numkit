// toolboxes/.../polynomials_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by polynomials.cpp + polynomials_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/ops/reductions.hpp>  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::builtin {

namespace {

ScratchVec<double> readPolyAsDouble(const Value &p, const char *fn, std::pmr::memory_resource *mr)
{
    if (p.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": complex coefficient input is not supported",
                     0, 0, fn, "", std::string("numkit:") + fn + ":complex");
    if (!p.dims().isVector() && !p.isScalar() && !p.isEmpty())
        throw Error(std::string(fn) + ": argument must be a vector",
                     0, 0, fn, "", std::string("numkit:") + fn + ":notVector");
    const std::size_t n = p.numel();
    ScratchVec<double> v(n, mr);
    for (std::size_t i = 0; i < n; ++i) v[i] = p.elemAsDouble(i);
    return v;
}

// Convolve two real polynomial coefficient vectors (length-N + length-M
// → length-N+M-1). Pointer + size for inputs so the same helper composes
// with std::vector and std::pmr::vector backings.
ScratchVec<double> polyConv(const double *a, std::size_t na, const double *b, std::size_t nb, std::pmr::memory_resource *mr)
{
    if (na == 0 || nb == 0) return ScratchVec<double>(mr);
    ScratchVec<double> r(na + nb - 1, mr);
    for (std::size_t i = 0; i < na; ++i)
        for (std::size_t j = 0; j < nb; ++j)
            r[i + j] += a[i] * b[j];
    return r;
}

// d/dx of a coefficient vector in MATLAB order.
ScratchVec<double> polyderRaw(const double *p, std::size_t pn, std::pmr::memory_resource *mr)
{
    if (pn <= 1) {
        ScratchVec<double> r(1, mr);
        r[0] = 0.0;  // constant → derivative is [0].
        return r;
    }
    const std::size_t n = pn - 1;  // degree
    ScratchVec<double> r(n, mr);
    for (std::size_t i = 0; i < n; ++i) {
        const double exponent = static_cast<double>(n - i);
        r[i] = p[i] * exponent;
    }
    return r;
}

Value rowFromVec(const double *v, std::size_t n, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(1, n, ValueType::DOUBLE, mr);
    if (n > 0)
        std::memcpy(out.doubleDataMut(), v, n * sizeof(double));
    return out;
}

// Trim trailing zeros that arise from a-b cancellation in polyder(b,a).
void trimLeadingZeros(ScratchVec<double> &v)
{
    std::size_t lo = 0;
    while (lo + 1 < v.size() && v[lo] == 0.0) ++lo;
    if (lo > 0) v.erase(v.begin(), v.begin() + lo);
}

} // namespace
namespace {

ScratchVec<numkit::ops::Complex> readVecAsComplex(const Value &v, const char *fn, std::pmr::memory_resource *mr)
{
    if (!v.dims().isVector() && !v.isScalar() && !v.isEmpty())
        throw Error(std::string(fn) + ": argument must be a vector",
                     0, 0, fn, "", std::string("numkit:") + fn + ":notVector");
    const std::size_t n = v.numel();
    ScratchVec<numkit::ops::Complex> r(n, mr);
    if (v.type() == ValueType::COMPLEX) {
        const auto *p = v.complexData();
        for (std::size_t i = 0; i < n; ++i) r[i] = p[i];
    } else {
        for (std::size_t i = 0; i < n; ++i)
            r[i] = numkit::ops::Complex(v.elemAsDouble(i), 0.0);
    }
    return r;
}

Value complexColFromVec(const numkit::ops::Complex *v, std::size_t n, std::pmr::memory_resource *mr)
{
    auto out = Value::complexMatrix(n, 1, mr);
    for (std::size_t i = 0; i < n; ++i)
        out.complexDataMut()[i] = v[i];
    return out;
}

Value realColIfFlat(const numkit::ops::Complex *v, std::size_t n, std::pmr::memory_resource *mr)
{
    bool anyComplex = false;
    for (std::size_t i = 0; i < n; ++i)
        if (std::abs(v[i].imag()) > 1e-12 * (std::abs(v[i].real()) + 1.0)) {
            anyComplex = true;
            break;
        }
    if (!anyComplex) {
        auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < n; ++i)
            out.doubleDataMut()[i] = v[i].real();
        return out;
    }
    return complexColFromVec(v, n, mr);
}

} // namespace

} // namespace numkit::builtin
