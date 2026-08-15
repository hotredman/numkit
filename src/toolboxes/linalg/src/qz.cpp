// toolboxes/linalg/src/qz.cpp
//
// qz — Generalized Schur decomposition [AA, BB, Q, Z] = qz(A, B).

#include <numkit/linalg/qz.hpp>
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

// Compute Givens rotation G = [c, s; -conj(s), c] such that G * [x; y] = [r; 0]
void makeGivensLeft(Complex x, Complex y, double &c, Complex &s)
{
    double norm_y = std::abs(y);
    if (norm_y == 0.0) {
        c = 1.0;
        s = Complex(0.0, 0.0);
        return;
    }
    double norm_x = std::abs(x);
    double r = std::hypot(norm_x, norm_y);
    c = norm_x / r;
    Complex phase = (norm_x > 0.0) ? (x / norm_x) : Complex(1.0, 0.0);
    s = phase * std::conj(y) / r;
}

// Compute Givens rotation G = [c, s; -conj(s), c] such that [x, y] * G = [0, r]
void makeGivensRight(Complex x, Complex y, double &c, Complex &s)
{
    double norm_x = std::abs(x);
    if (norm_x == 0.0) {
        c = 1.0;
        s = Complex(0.0, 0.0);
        return;
    }
    double norm_y = std::abs(y);
    double r = std::hypot(norm_x, norm_y);
    c = norm_y / r;
    Complex phase = (norm_y > 0.0) ? (y / norm_y) : Complex(1.0, 0.0);
    s = -phase * std::conj(x) / r;
}

void applyGivensLeft(Complex *M, std::size_t n, std::size_t row1, std::size_t row2, double c, Complex s, std::size_t col_start = 0)
{
    Complex G00 = c;
    Complex G01 = s;
    Complex G10 = -std::conj(s);
    Complex G11 = c;

    for (std::size_t k = col_start; k < n; ++k) {
        Complex r0 = M[row1 + k * n];
        Complex r1 = M[row2 + k * n];
        M[row1 + k * n] = G00 * r0 + G01 * r1;
        M[row2 + k * n] = G10 * r0 + G11 * r1;
    }
}

void applyGivensRight(Complex *M, std::size_t n, std::size_t col1, std::size_t col2, double c, Complex s, std::size_t row_end = 0)
{
    if (row_end == 0) row_end = n;
    Complex GH00 = c;
    Complex GH01 = -s;
    Complex GH10 = std::conj(s);
    Complex GH11 = c;

    for (std::size_t i = 0; i < row_end; ++i) {
        Complex c0 = M[i + col1 * n];
        Complex c1 = M[i + col2 * n];
        M[i + col1 * n] = c0 * GH00 + c1 * GH10;
        M[i + col2 * n] = c0 * GH01 + c1 * GH11;
    }
}

} // anonymous namespace

std::tuple<Value, Value, Value, Value> qz(const Value &A, const Value &B,
                                           std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2 || B.dims().ndim() != 2)
        throw Error("qz: A and B must be 2D matrices", 0, 0, "qz", "", "numkit:qz:notMatrix");
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
    if (n != static_cast<std::size_t>(A.dims().dim(1)) ||
        static_cast<std::size_t>(B.dims().dim(0)) != n ||
        static_cast<std::size_t>(B.dims().dim(1)) != n)
        throw Error("qz: A and B must be square matrices of the same size", 0, 0, "qz", "", "numkit:qz:badDims");

    if (n == 0) {
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr)};
    }

    ScratchArena scratch(mr);
    ScratchVec<Complex> Ac(n * n, &scratch);
    ScratchVec<Complex> Bc(n * n, &scratch);
    ScratchVec<Complex> Qc(n * n, Complex(0.0, 0.0), &scratch);
    ScratchVec<Complex> Zc(n * n, Complex(0.0, 0.0), &scratch);

    for (std::size_t i = 0; i < n; ++i) {
        Qc[i + i * n] = Complex(1.0, 0.0);
        Zc[i + i * n] = Complex(1.0, 0.0);
    }

    if (A.isComplex()) {
        std::copy(A.complexData(), A.complexData() + n * n, Ac.begin());
    } else {
        const double *ad = A.doubleData();
        for (std::size_t i = 0; i < n * n; ++i) Ac[i] = Complex(ad[i], 0.0);
    }

    if (B.isComplex()) {
        std::copy(B.complexData(), B.complexData() + n * n, Bc.begin());
    } else {
        const double *bd = B.doubleData();
        for (std::size_t i = 0; i < n * n; ++i) Bc[i] = Complex(bd[i], 0.0);
    }

    // Step 1: Reduce B to upper-triangular using QR and apply to A, Q
    for (std::size_t k = 0; k < n; ++k) {
        for (std::intptr_t i = static_cast<std::intptr_t>(n) - 1; i > static_cast<std::intptr_t>(k); --i) {
            std::size_t ui = static_cast<std::size_t>(i);
            if (std::abs(Bc[ui + k * n]) > 1e-15) {
                double c; Complex s;
                makeGivensLeft(Bc[ui - 1 + k * n], Bc[ui + k * n], c, s);
                applyGivensLeft(Bc.data(), n, ui - 1, ui, c, s, k);
                applyGivensLeft(Ac.data(), n, ui - 1, ui, c, s, 0);
                applyGivensLeft(Qc.data(), n, ui - 1, ui, c, s, 0);
            }
        }
    }

    // Step 2: Reduce A to upper Hessenberg while keeping B upper triangular
    for (std::size_t j = 0; j < n; ++j) {
        for (std::intptr_t i = static_cast<std::intptr_t>(n) - 1; i >= static_cast<std::intptr_t>(j) + 2; --i) {
            std::size_t ui = static_cast<std::size_t>(i);
            if (std::abs(Ac[ui + j * n]) > 1e-15) {
                // Zero Ac(ui, j) via left Givens on rows ui-1, ui
                double c; Complex s;
                makeGivensLeft(Ac[ui - 1 + j * n], Ac[ui + j * n], c, s);
                applyGivensLeft(Ac.data(), n, ui - 1, ui, c, s, j);
                applyGivensLeft(Bc.data(), n, ui - 1, ui, c, s, ui - 1);
                applyGivensLeft(Qc.data(), n, ui - 1, ui, c, s, 0);

                // Zero fill-in entry Bc(ui, ui-1) via right Givens on cols ui-1, ui
                if (std::abs(Bc[ui + (ui - 1) * n]) > 1e-15) {
                    double cr; Complex sr;
                    makeGivensRight(Bc[ui + (ui - 1) * n], Bc[ui + ui * n], cr, sr);
                    applyGivensRight(Bc.data(), n, ui - 1, ui, cr, sr, ui + 1);
                    applyGivensRight(Ac.data(), n, ui - 1, ui, cr, sr, n);
                    applyGivensRight(Zc.data(), n, ui - 1, ui, cr, sr, n);
                }
            }
        }
    }

    // Step 3: QZ iteration with Wilkinson shift to reduce subdiagonals of A to 0
    std::size_t max_iter = 100 * n;
    bool converged = false;

    for (std::size_t iter = 0; iter < max_iter; ++iter) {
        bool all_zero = true;
        for (std::size_t k = 0; k < n - 1; ++k) {
            double sub = std::abs(Ac[k + 1 + k * n]);
            double diag = std::abs(Ac[k + k * n]) + std::abs(Ac[k + 1 + (k + 1) * n]);
            if (sub > 1e-14 * diag) {
                all_zero = false;

                // 2x2 Wilkinson shift for block k:k+1
                Complex a11 = Ac[k + k * n],       a12 = Ac[k + (k + 1) * n];
                Complex a21 = Ac[k + 1 + k * n],   a22 = Ac[k + 1 + (k + 1) * n];
                Complex b11 = Bc[k + k * n],       b12 = Bc[k + (k + 1) * n];
                Complex b21 = Bc[k + 1 + k * n],   b22 = Bc[k + 1 + (k + 1) * n];

                Complex shift;
                if (iter > 0 && iter % 10 == 0) {
                    // Exceptional shift fallback
                    shift = (a22 / (std::abs(b22) > 1e-15 ? b22 : Complex(1e-15, 0.0))) + Complex(0.1 * iter, 0.1 * iter);
                } else {
                    // Solve det(A_2x2 - lambda * B_2x2) = 0
                    Complex c2 = b11 * b22 - b12 * b21;
                    Complex c1 = -(a11 * b22 + a22 * b11 - a12 * b21 - a21 * b12);
                    Complex c0 = a11 * a22 - a12 * a21;

                    if (std::abs(c2) > 1e-15) {
                        Complex disc = std::sqrt(c1 * c1 - Complex(4.0, 0.0) * c2 * c0);
                        Complex r1 = (-c1 + disc) / (Complex(2.0, 0.0) * c2);
                        Complex r2 = (-c1 - disc) / (Complex(2.0, 0.0) * c2);
                        Complex target = a22 / (std::abs(b22) > 1e-15 ? b22 : Complex(1e-15, 0.0));
                        shift = (std::abs(r1 - target) < std::abs(r2 - target)) ? r1 : r2;
                    } else {
                        shift = a22 / (std::abs(b22) > 1e-15 ? b22 : Complex(1e-15, 0.0));
                    }
                }

                Complex x = Ac[k + k * n] - shift * Bc[k + k * n];
                Complex y = Ac[k + 1 + k * n];

                double c; Complex s;
                makeGivensLeft(x, y, c, s);
                applyGivensLeft(Ac.data(), n, k, k + 1, c, s, k);
                applyGivensLeft(Bc.data(), n, k, k + 1, c, s, k);
                applyGivensLeft(Qc.data(), n, k, k + 1, c, s, 0);

                // Zero out fill-in in Bc(k+1, k)
                if (std::abs(Bc[k + 1 + k * n]) > 1e-15) {
                    double cr; Complex sr;
                    makeGivensRight(Bc[k + 1 + k * n], Bc[k + 1 + (k + 1) * n], cr, sr);
                    applyGivensRight(Bc.data(), n, k, k + 1, cr, sr, k + 2);
                    applyGivensRight(Ac.data(), n, k, k + 1, cr, sr, n);
                    applyGivensRight(Zc.data(), n, k, k + 1, cr, sr, n);
                }
            } else {
                Ac[k + 1 + k * n] = Complex(0.0, 0.0);
            }
        }
        if (all_zero) {
            converged = true;
            break;
        }
    }

    if (!converged && n > 1) {
        throw Error("qz: QZ iteration failed to converge", 0, 0, "qz", "", "numkit:qz:noConverge");
    }

    // Zero out small subdiagonals in Ac and Bc
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            Ac[i + j * n] = Complex(0.0, 0.0);
            Bc[i + j * n] = Complex(0.0, 0.0);
        }
    }

    auto AA_out = Value::complexMatrix(n, n, mr);
    auto BB_out = Value::complexMatrix(n, n, mr);
    auto Q_out  = Value::complexMatrix(n, n, mr);
    auto Z_out  = Value::complexMatrix(n, n, mr);

    std::copy(Ac.begin(), Ac.end(), AA_out.complexDataMut());
    std::copy(Bc.begin(), Bc.end(), BB_out.complexDataMut());
    std::copy(Qc.begin(), Qc.end(), Q_out.complexDataMut());
    std::copy(Zc.begin(), Zc.end(), Zc.begin());
    std::copy(Zc.begin(), Zc.end(), Z_out.complexDataMut());

    return {detail::narrow_if_real(AA_out, mr),
            detail::narrow_if_real(BB_out, mr),
            detail::narrow_if_real(Q_out, mr),
            detail::narrow_if_real(Z_out, mr)};
}

} // namespace numkit::linalg
