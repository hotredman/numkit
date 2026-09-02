// toolboxes/linalg/src/ordqz.cpp
//
// ordqz — Reorder Generalized Schur decomposition Q * AA * Z = AA, Q * BB * Z = BB.

#include <numkit/linalg/ordqz.hpp>
#include "linalg_detail.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace numkit::linalg {

namespace {

using Complex = std::complex<double>;

void swapAdjacentPencil(Complex *AA, Complex *BB, Complex *Q, Complex *Z, std::size_t n, std::size_t j)
{
    Complex a11 = AA[j + j * n];
    Complex a22 = AA[j + 1 + (j + 1) * n];
    Complex a12 = AA[j + (j + 1) * n];

    Complex b11 = BB[j + j * n];
    Complex b22 = BB[j + 1 + (j + 1) * n];
    Complex b12 = BB[j + (j + 1) * n];

    // Compute right rotation (z1, z2) to zero out bottom-left of (A*b22 - B*a22)
    Complex v1 = a11 * b22 - a22 * b11;
    Complex v2 = a12 * b22 - b12 * a22;

    double c_z = 1.0;
    Complex s_z = Complex(0.0, 0.0);
    double norm_v2 = std::abs(v2);

    if (norm_v2 > 1e-15) {
        double norm_v1 = std::abs(v1);
        double r = std::hypot(norm_v1, norm_v2);
        c_z = norm_v1 / r;
        Complex phase = (norm_v1 > 0.0) ? (v1 / norm_v1) : Complex(1.0, 0.0);
        s_z = phase * std::conj(v2) / r;
    }

    Complex Z00 = c_z;
    Complex Z01 = s_z;
    Complex Z10 = -std::conj(s_z);
    Complex Z11 = c_z;

    // Apply Z rotation from right on cols j and j+1 of AA, BB, Z
    for (std::size_t i = 0; i < n; ++i) {
        Complex c0a = AA[i + j * n];
        Complex c1a = AA[i + (j + 1) * n];
        AA[i + j * n]       = c0a * Z00 + c1a * Z10;
        AA[i + (j + 1) * n] = c0a * Z01 + c1a * Z11;

        Complex c0b = BB[i + j * n];
        Complex c1b = BB[i + (j + 1) * n];
        BB[i + j * n]       = c0b * Z00 + c1b * Z10;
        BB[i + (j + 1) * n] = c0b * Z01 + c1b * Z11;

        Complex c0z = Z[i + j * n];
        Complex c1z = Z[i + (j + 1) * n];
        Z[i + j * n]       = c0z * Z00 + c1z * Z10;
        Z[i + (j + 1) * n] = c0z * Z01 + c1z * Z11;
    }

    // Left rotation to eliminate subdiagonal BB(j+1, j) created by Z
    Complex b11_new = BB[j + j * n];
    Complex b21_new = BB[j + 1 + j * n];

    double c_q = 1.0;
    Complex s_q = Complex(0.0, 0.0);
    double norm_b21 = std::abs(b21_new);

    if (norm_b21 > 1e-15) {
        double norm_b11 = std::abs(b11_new);
        double r = std::hypot(norm_b11, norm_b21);
        c_q = norm_b11 / r;
        Complex phase = (norm_b11 > 0.0) ? (b11_new / norm_b11) : Complex(1.0, 0.0);
        s_q = phase * std::conj(b21_new) / r;
    }

    Complex Q00 = c_q;
    Complex Q01 = s_q;
    Complex Q10 = -std::conj(s_q);
    Complex Q11 = c_q;

    // Apply Q to rows j and j+1 of AA, BB, Q
    for (std::size_t k = 0; k < n; ++k) {
        Complex r0a = AA[j + k * n];
        Complex r1a = AA[j + 1 + k * n];
        AA[j + k * n]     = Q00 * r0a + Q01 * r1a;
        AA[j + 1 + k * n] = Q10 * r0a + Q11 * r1a;

        Complex r0b = BB[j + k * n];
        Complex r1b = BB[j + 1 + k * n];
        BB[j + k * n]     = Q00 * r0b + Q01 * r1b;
        BB[j + 1 + k * n] = Q10 * r0b + Q11 * r1b;

        Complex r0q = Q[j + k * n];
        Complex r1q = Q[j + 1 + k * n];
        Q[j + k * n]     = Q00 * r0q + Q01 * r1q;
        Q[j + 1 + k * n] = Q10 * r0q + Q11 * r1q;
    }
}

} // anonymous namespace

std::tuple<Value, Value, Value, Value> ordqz(const Value &AA, const Value &BB,
                                              const Value &Q, const Value &Z,
                                              const Value &select,
                                              std::pmr::memory_resource *mr)
{
    if (AA.dims().ndim() != 2 || BB.dims().ndim() != 2 || Q.dims().ndim() != 2 || Z.dims().ndim() != 2)
        throw Error("ordqz: inputs must be 2D matrices", 0, 0, "ordqz", "", "numkit:ordqz:notMatrix");

    const std::size_t n = static_cast<std::size_t>(AA.dims().dim(0));
    if (n != static_cast<std::size_t>(AA.dims().dim(1)) ||
        static_cast<std::size_t>(BB.dims().dim(0)) != n || static_cast<std::size_t>(BB.dims().dim(1)) != n ||
        static_cast<std::size_t>(Q.dims().dim(0)) != n || static_cast<std::size_t>(Q.dims().dim(1)) != n ||
        static_cast<std::size_t>(Z.dims().dim(0)) != n || static_cast<std::size_t>(Z.dims().dim(1)) != n)
        throw Error("ordqz: AA, BB, Q, Z must be n x n square matrices", 0, 0, "ordqz", "", "numkit:ordqz:badDims");

    if (select.numel() != n)
        throw Error("ordqz: select must have length n", 0, 0, "ordqz", "", "numkit:ordqz:badSelect");

    if (n == 0) {
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr)};
    }

    Value AAout = Value::complexMatrix(n, n, mr);
    Value BBout = Value::complexMatrix(n, n, mr);
    Value Qout  = Value::complexMatrix(n, n, mr);
    Value Zout  = Value::complexMatrix(n, n, mr);

    Complex *aad = AAout.complexDataMut();
    Complex *bbd = BBout.complexDataMut();
    Complex *qd  = Qout.complexDataMut();
    Complex *zd  = Zout.complexDataMut();

    // Copy initial matrices as complex
    if (AA.isComplex()) {
        std::copy(AA.complexData(), AA.complexData() + n * n, aad);
    } else {
        for (std::size_t i = 0; i < n * n; ++i) aad[i] = Complex(AA.doubleData()[i], 0.0);
    }
    if (BB.isComplex()) {
        std::copy(BB.complexData(), BB.complexData() + n * n, bbd);
    } else {
        for (std::size_t i = 0; i < n * n; ++i) bbd[i] = Complex(BB.doubleData()[i], 0.0);
    }
    if (Q.isComplex()) {
        std::copy(Q.complexData(), Q.complexData() + n * n, qd);
    } else {
        for (std::size_t i = 0; i < n * n; ++i) qd[i] = Complex(Q.doubleData()[i], 0.0);
    }
    if (Z.isComplex()) {
        std::copy(Z.complexData(), Z.complexData() + n * n, zd);
    } else {
        for (std::size_t i = 0; i < n * n; ++i) zd[i] = Complex(Z.doubleData()[i], 0.0);
    }

    std::vector<uint8_t> sel(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        sel[i] = select.isLogical() ? select.logicalData()[i] : (select.elemAsDouble(i) != 0.0);
    }

    // Bubble selected eigenvalues to the top left
    std::size_t target = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (sel[i]) {
            for (std::size_t j = i; j > target; --j) {
                swapAdjacentPencil(aad, bbd, qd, zd, n, j - 1);
                std::swap(sel[j], sel[j - 1]);
            }
            target++;
        }
    }

    return {detail::narrow_if_real(AAout, mr),
            detail::narrow_if_real(BBout, mr),
            detail::narrow_if_real(Qout, mr),
            detail::narrow_if_real(Zout, mr)};
}

std::tuple<Value, Value, Value, Value> ordqz(const Value &AA, const Value &BB,
                                              const Value &Q, const Value &Z,
                                              const std::string &domain,
                                              std::pmr::memory_resource *mr)
{
    const std::size_t n = static_cast<std::size_t>(AA.dims().dim(0));
    auto select = Value::matrix(1, n, ValueType::LOGICAL, mr);
    std::uint8_t *sd = select.logicalDataMut();

    for (std::size_t i = 0; i < n; ++i) {
        Complex a = AA.isComplex() ? AA.complexData()[i + i * n] : Complex(AA.doubleData()[i + i * n], 0.0);
        Complex b = BB.isComplex() ? BB.complexData()[i + i * n] : Complex(BB.doubleData()[i + i * n], 0.0);
        Complex lambda = (b != Complex(0.0, 0.0)) ? (a / b) : Complex(1e15, 0.0);

        if (domain == "lhp") {
            sd[i] = (lambda.real() < 0.0) ? 1 : 0;
        } else if (domain == "rhp") {
            sd[i] = (lambda.real() > 0.0) ? 1 : 0;
        } else if (domain == "uip") {
            sd[i] = (std::abs(lambda) < 1.0) ? 1 : 0;
        } else if (domain == "uop") {
            sd[i] = (std::abs(lambda) > 1.0) ? 1 : 0;
        } else {
            throw Error("ordqz: unknown domain '" + domain + "'", 0, 0, "ordqz", "", "numkit:ordqz:badDomain");
        }
    }

    return ordqz(AA, BB, Q, Z, select, mr);
}

} // namespace numkit::linalg
