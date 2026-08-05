// toolboxes/linalg/tests/qr_complex_test.cpp
//
// Unit tests for complex QR decomposition (unpivoted, pivoted) and rectangular least squares.

#include <gtest/gtest.h>
#include <numkit/linalg/decompositions.hpp>
#include "../src/decompositions_detail.hpp"
#include <numkit/linalg/solvers.hpp>
#include <numkit/value/value.hpp>

#include <complex>

using namespace numkit;
using namespace numkit::linalg;

TEST(ComplexQrTest, QrUnpivotedComplex3x2) {
    // A = [1+1i 2; 3 4-1i; 0 1+2i] (3x2)
    Value A = Value::complexMatrix(3, 2);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(1.0, 1.0);  // A(1,1)
    ad[1] = std::complex<double>(3.0, 0.0);  // A(2,1)
    ad[2] = std::complex<double>(0.0, 0.0);  // A(3,1)
    ad[3] = std::complex<double>(2.0, 0.0);  // A(1,2)
    ad[4] = std::complex<double>(4.0, -1.0); // A(2,2)
    ad[5] = std::complex<double>(1.0, 2.0);  // A(3,2)

    auto [Q, R] = qr_decompose(A);
    EXPECT_EQ(Q.dims().rows(), 3);
    EXPECT_EQ(Q.dims().cols(), 3);
    EXPECT_EQ(R.dims().rows(), 3);
    EXPECT_EQ(R.dims().cols(), 2);

    auto getQ = [&](size_t r, size_t c) -> std::complex<double> {
        return Q.isComplex() ? Q.complexData()[r + c * 3] : std::complex<double>(Q.doubleData()[r + c * 3], 0.0);
    };
    auto getR = [&](size_t r, size_t c) -> std::complex<double> {
        return R.isComplex() ? R.complexData()[r + c * 3] : std::complex<double>(R.doubleData()[r + c * 3], 0.0);
    };

    // Verify reconstruction Q * R = A
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 3; ++k) {
                sum += getQ(i, k) * getR(k, j);
            }
            EXPECT_NEAR(sum.real(), ad[i + j * 3].real(), 1e-14);
            EXPECT_NEAR(sum.imag(), ad[i + j * 3].imag(), 1e-14);
        }
    }

    // Verify unitarity Q^H * Q = I_3
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 3; ++k) {
                sum += std::conj(getQ(k, i)) * getQ(k, j);
            }
            double expected_re = (i == j) ? 1.0 : 0.0;
            EXPECT_NEAR(sum.real(), expected_re, 1e-14);
            EXPECT_NEAR(sum.imag(), 0.0, 1e-14);
        }
    }
}

TEST(ComplexQrTest, QrPivotedComplex3x2) {
    // A = [1+1i 2; 3 4-1i; 0 1+2i] (3x2)
    Value A = Value::complexMatrix(3, 2);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(1.0, 1.0);
    ad[1] = std::complex<double>(3.0, 0.0);
    ad[2] = std::complex<double>(0.0, 0.0);
    ad[3] = std::complex<double>(2.0, 0.0);
    ad[4] = std::complex<double>(4.0, -1.0);
    ad[5] = std::complex<double>(1.0, 2.0);

    std::vector<size_t> perm;
    auto [Q, R] = qr_pivoted(A, perm);
    EXPECT_EQ(perm.size(), 2);

    // Verify A * P = Q * R
    Value AP = Value::complexMatrix(3, 2);
    auto *apd = AP.complexDataMut();
    for (size_t j = 0; j < 2; ++j) {
        size_t orig_col = perm[j];
        for (size_t i = 0; i < 3; ++i) {
            apd[i + j * 3] = ad[i + orig_col * 3];
        }
    }

    auto getQ = [&](size_t r, size_t c) -> std::complex<double> {
        return Q.isComplex() ? Q.complexData()[r + c * 3] : std::complex<double>(Q.doubleData()[r + c * 3], 0.0);
    };
    auto getR = [&](size_t r, size_t c) -> std::complex<double> {
        return R.isComplex() ? R.complexData()[r + c * 3] : std::complex<double>(R.doubleData()[r + c * 3], 0.0);
    };

    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 3; ++k) {
                sum += getQ(i, k) * getR(k, j);
            }
            EXPECT_NEAR(sum.real(), apd[i + j * 3].real(), 1e-14);
            EXPECT_NEAR(sum.imag(), apd[i + j * 3].imag(), 1e-14);
        }
    }
}

TEST(ComplexQrTest, MldivideComplexRectangular) {
    // A = [1+1i 2; 3 4-1i; 0 1+2i] (3x2)
    // b = [1; 2+1i; 3-1i] (3x1)
    Value A = Value::complexMatrix(3, 2);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(1.0, 1.0);
    ad[1] = std::complex<double>(3.0, 0.0);
    ad[2] = std::complex<double>(0.0, 0.0);
    ad[3] = std::complex<double>(2.0, 0.0);
    ad[4] = std::complex<double>(4.0, -1.0);
    ad[5] = std::complex<double>(1.0, 2.0);

    Value b = Value::complexMatrix(3, 1);
    auto *bd = b.complexDataMut();
    bd[0] = std::complex<double>(1.0, 0.0);
    bd[1] = std::complex<double>(2.0, 1.0);
    bd[2] = std::complex<double>(3.0, -1.0);

    Value x = linsolve(A, b);
    ASSERT_TRUE(x.isComplex());
    const auto *xd = x.complexData();

    // Normal equation residual A^H * (A*x - b) == 0
    std::complex<double> res[3];
    for (size_t i = 0; i < 3; ++i) {
        std::complex<double> ax(0.0, 0.0);
        for (size_t k = 0; k < 2; ++k) {
            ax += ad[i + k * 3] * xd[k];
        }
        res[i] = ax - bd[i];
    }

    for (size_t j = 0; j < 2; ++j) {
        std::complex<double> ah_res(0.0, 0.0);
        for (size_t i = 0; i < 3; ++i) {
            ah_res += std::conj(ad[i + j * 3]) * res[i];
        }
        EXPECT_NEAR(ah_res.real(), 0.0, 1e-13);
        EXPECT_NEAR(ah_res.imag(), 0.0, 1e-13);
    }
}
