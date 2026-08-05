// toolboxes/linalg/src/ordschur.cpp
//
// ordschur — Reorder Schur decomposition A = U * T * U^H.

#include <numkit/linalg/ordschur.hpp>
#include "linalg_detail.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::linalg {

namespace {

using Complex = std::complex<double>;

void swapAdjacent(Complex *T, Complex *U, std::size_t n, std::size_t j)
{
    // Diagonal entries: d1 = T[j,j], d2 = T[j+1,j+1], t12 = T[j,j+1]
    Complex d1 = T[j + j * n];
    Complex d2 = T[j + 1 + (j + 1) * n];
    Complex t12 = T[j + (j + 1) * n];

    Complex x = d1 - d2;
    Complex y = -t12;

    double c = 1.0;
    Complex s = Complex(0.0, 0.0);

    double norm_y = std::abs(y);
    if (norm_y > 1e-15) {
        double norm_x = std::abs(x);
        double r = std::hypot(norm_x, norm_y);
        c = norm_x / r;
        Complex phase = (norm_x > 0.0) ? (x / norm_x) : Complex(1.0, 0.0);
        s = phase * std::conj(y) / r;
    }

    // G = [c, s; -conj(s), c]
    Complex G00 = c;
    Complex G01 = s;
    Complex G10 = -std::conj(s);
    Complex G11 = c;

    // Apply G to rows j and j+1 of T: T(j:j+1, :) = G * T(j:j+1, :)
    for (std::size_t k = j; k < n; ++k) {
        Complex r0 = T[j + k * n];
        Complex r1 = T[j + 1 + k * n];
        T[j + k * n]     = G00 * r0 + G01 * r1;
        T[j + 1 + k * n] = G10 * r0 + G11 * r1;
    }

    // Apply G^H to cols j and j+1 of T: T(:, j:j+1) = T(:, j:j+1) * G^H
    // G^H = [conj(G00), conj(G10); conj(G01), conj(G11)]
    Complex GH00 = std::conj(G00);
    Complex GH01 = std::conj(G10);
    Complex GH10 = std::conj(G01);
    Complex GH11 = std::conj(G11);

    for (std::size_t i = 0; i <= j + 1; ++i) {
        Complex c0 = T[i + j * n];
        Complex c1 = T[i + (j + 1) * n];
        T[i + j * n]       = c0 * GH00 + c1 * GH10;
        T[i + (j + 1) * n] = c0 * GH01 + c1 * GH11;
    }

    // Apply G^H to cols j and j+1 of U: U(:, j:j+1) = U(:, j:j+1) * G^H
    for (std::size_t i = 0; i < n; ++i) {
        Complex c0 = U[i + j * n];
        Complex c1 = U[i + (j + 1) * n];
        U[i + j * n]       = c0 * GH00 + c1 * GH10;
        U[i + (j + 1) * n] = c0 * GH01 + c1 * GH11;
    }
}

} // anonymous namespace

std::tuple<Value, Value> ordschur(const Value &U, const Value &T,
                                  const Value &select,
                                  std::pmr::memory_resource *mr)
{
    if (U.dims().ndim() != 2 || T.dims().ndim() != 2)
        throw Error("ordschur: U and T must be 2D matrices", 0, 0, "ordschur", "", "numkit:ordschur:notMatrix");
    const std::size_t n = static_cast<std::size_t>(T.dims().dim(0));
    if (n != static_cast<std::size_t>(T.dims().dim(1)) ||
        static_cast<std::size_t>(U.dims().dim(0)) != n ||
        static_cast<std::size_t>(U.dims().dim(1)) != n)
        throw Error("ordschur: U and T must be n x n square matrices", 0, 0, "ordschur", "", "numkit:ordschur:badDims");

    if (select.numel() != n)
        throw Error("ordschur: select must have length n", 0, 0, "ordschur", "", "numkit:ordschur:badSelect");

    if (n == 0) {
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr)};
    }

    ScratchArena scratch(mr);
    ScratchVec<Complex> Tc(n * n, &scratch);
    ScratchVec<Complex> Uc(n * n, &scratch);
    ScratchVec<std::uint8_t> sel(n, &scratch);

    if (T.isComplex()) {
        std::copy(T.complexData(), T.complexData() + n * n, Tc.begin());
    } else {
        const double *td = T.doubleData();
        for (std::size_t i = 0; i < n * n; ++i) Tc[i] = Complex(td[i], 0.0);
    }

    if (U.isComplex()) {
        std::copy(U.complexData(), U.complexData() + n * n, Uc.begin());
    } else {
        const double *ud = U.doubleData();
        for (std::size_t i = 0; i < n * n; ++i) Uc[i] = Complex(ud[i], 0.0);
    }

    if (select.type() == ValueType::LOGICAL) {
        const uint8_t *ld = select.logicalData();
        for (std::size_t i = 0; i < n; ++i) sel[i] = (ld[i] != 0) ? 1 : 0;
    } else {
        for (std::size_t i = 0; i < n; ++i) sel[i] = (select.elemAsDouble(i) != 0.0) ? 1 : 0;
    }

    // Bubble-sort selected eigenvalues to the top-left
    for (std::size_t i = 0; i < n; ++i) {
        for (std::intptr_t j = static_cast<std::intptr_t>(n) - 2; j >= static_cast<std::intptr_t>(i); --j) {
            std::size_t uj = static_cast<std::size_t>(j);
            if (sel[uj + 1] && !sel[uj]) {
                swapAdjacent(Tc.data(), Uc.data(), n, uj);
                std::swap(sel[uj], sel[uj + 1]);
            }
        }
    }

    auto U_out = Value::complexMatrix(n, n, mr);
    auto T_out = Value::complexMatrix(n, n, mr);
    std::copy(Uc.begin(), Uc.end(), U_out.complexDataMut());
    std::copy(Tc.begin(), Tc.end(), T_out.complexDataMut());

    return {detail::narrow_if_real(U_out, mr), detail::narrow_if_real(T_out, mr)};
}

std::tuple<Value, Value> ordschur(const Value &U, const Value &T,
                                  const std::string &domain,
                                  std::pmr::memory_resource *mr)
{
    const std::size_t n = static_cast<std::size_t>(T.dims().dim(0));
    Value select = Value::matrix(1, n, ValueType::LOGICAL, mr);
    uint8_t *sd = select.logicalDataMut();

    for (std::size_t i = 0; i < n; ++i) {
        Complex val = T.isComplex() ? T.complexData()[i + i * n] : Complex(T.doubleData()[i + i * n], 0.0);
        bool s = false;
        if (domain == "lhp") {
            s = (val.real() < 0.0);
        } else if (domain == "rhp") {
            s = (val.real() > 0.0);
        } else if (domain == "uip") {
            s = (std::abs(val) < 1.0);
        } else if (domain == "uop") {
            s = (std::abs(val) > 1.0);
        } else {
            throw Error("ordschur: unknown domain keyword '" + domain + "'", 0, 0, "ordschur", "", "numkit:ordschur:badDomain");
        }
        sd[i] = s ? 1 : 0;
    }

    return ordschur(U, T, select, mr);
}

} // namespace numkit::linalg
